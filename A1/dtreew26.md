1. main 里解析参数：看 argv[1] 是哪个模式（比如 -dircnt）。
2. 把用户输入保存到全局/上下文：比如要找的文件名、要统计的扩展名、源/目标目录等。
3. 调用 nftw(root, callback, fd_limit, flags) 遍历目录树。
4. callback 里做两种事之一：
   • 计数/累加（不需要排序的功能）：边走边加，最后输出即可。
   • 收集列表（需要排序/输出清单的功能）：把每个符合条件的文件信息存进数组，遍历结束后 qsort 排序再打印。
5. 如果是复制/移动/删除：callback 里对每个匹配文件做对应操作（或先收集后统一操作）。

把这 10 个功能都当成：“遍历整棵树 → 对每个路径判断是否符合条件 → 做动作/记录”。

⸻

A) -flist [dir]：按时间倒序列出文件

**目标：**列出 dir 下所有文件，按“时间从新到旧”。

实现思路：
• nftw(dir, callback, ...) 遍历。
• callback 里：如果是 普通文件（regular file），就把它的
• path（绝对路径）
• time（用 stat 的时间字段，比如 st_mtime）
存到数组里。
• 遍历结束：qsort 按 time 降序排序，打印 path。

⸻

B) -tcount [ext1] [ext2] [ext3] [dir]：统计指定扩展名数量

**目标：**统计 .c .txt .pdf 这种扩展名各有多少。

实现思路：
• 先把 ext1~ext3 存起来（可能只有 1–3 个）。
• nftw(dir, callback...)
• callback 里：如果是普通文件：
• 取文件名的后缀（例如找最后一个 . 之后的部分，或者直接判断结尾是否匹配 .txt）
• 如果匹配 ext1 → count1++；匹配 ext2 → count2++；匹配 ext3 → count3++
• 结束后打印三行（或按输入的 ext 数量打印）。

⸻

C) -srchf [filename] [root_dir]：在子树里找同名文件

**目标：**在 root_dir 下面找“文件名恰好等于 filename”的文件，输出所有绝对路径。

实现思路：
• nftw(root_dir, callback...)
• callback 里：如果是普通文件：
• 从 fpath 里取 basename（最后一个 / 后面那段）
• 如果 basename == filename → 打印 fpath（或存数组最后统一打印）
• 最后如果一次都没找到，输出 “Not found”。

⸻

D) -dircnt [root_dir]：统计目录数量

**目标：**统计子树里有多少个目录。

实现思路：
• nftw(root_dir, callback...)
• callback 里：如果当前项是 目录 → dirCount++
• 结束打印 dirCount。

是否包含 root_dir 自己：一般 nftw 会先访问根目录，你的计数通常会包含它。你可以在输出时说明。

⸻

E) -sumfilesize [root_dir]：统计总文件大小

**目标：**把子树中所有文件的大小加起来。

实现思路：
• nftw(root_dir, callback...)
• callback 里：如果是普通文件：
• total += statbuf->st_size
• 结束打印 total（字节）。

⸻

F) -lfsize [dir]：按大小从大到小列出文件（大小相同按文件名）

**目标：**列出子树里所有文件：绝对路径 + size，按 size 降序；size 相同按名字字母序。

实现思路：
• nftw(dir, callback...)
• callback 里：如果普通文件：
• 存 path、size、basename（为了比较时用）
• 遍历结束：qsort
• 主排序：size 大的在前（降序）
• 平局：basename 字母序升序
• 打印每条：path size

⸻

G) -nonwr [dir]：列出没有写权限的文件（按字母序）

**目标：**找出用户“写不了”的文件，排序后输出。

实现思路：
• nftw(dir, callback...)
• callback 里：如果普通文件：
• 用 access(fpath, W_OK) 判断：返回非 0 表示不可写
• 不可写就把 path 存进数组
• 遍历结束：qsort 按 path 字母序
• 输出路径列表。

⸻

H) -copyd [source_dir] [destination_dir]：复制整个目录树

**目标：**把 source_dir 整棵复制到 destination_dir 下，源不删，结构一致。

最简单通俗的做法：
• 先确定“目标根路径”：
• 假设 source_dir 的目录名叫 chapter4
• 目标根就是 destination_dir/chapter4
• nftw(source_dir, callback...) 遍历 source。
• callback 里对每个路径做：1. 算出相对路径 rel（比如 source_dir/abc/1.txt 的 rel 是 /abc/1.txt）2. 得到目标路径 dst = destination_dir + "/" + basename(source_dir) + rel 3. 如果当前是目录：mkdir(dst, ...)（不存在就创建）4. 如果是文件：把源文件内容复制到 dst
• 最简单：open(src) -> read -> write -> close

复制目录树就是：遇到目录就建目录，遇到文件就复制文件。

⸻

I) -dmove [source_dir] [destination_dir]：移动整个目录树（搬走并删除源）

**目标：**效果像剪切粘贴：目的地有一份，源目录消失。

最稳的实现思路：
• 先用上面 H 的逻辑 完整复制一份 到目的地。
• 如果复制成功，再第二次 nftw(source_dir, ...) 做删除：
• 删除要用 “从最深处开始”（先删文件，再删空目录）
• nftw 有个 flag 可以让你后序遍历（常见做法：让回调在目录内容处理完后再处理目录），这样不会出现“目录还没空就删目录失败”的问题。
• callback 里：
• 普通文件：unlink(path)
• 目录：rmdir(path)
• 最后 source_dir 没了，就完成 move。

“移动 = 复制 + 删除源”，实现最简单也最不容易出错。

⸻

J) -remd [root_dir] [file_extension]：删除指定扩展名文件

**目标：**在子树里删除所有 .log 这种扩展名文件。

实现思路：
• nftw(root_dir, callback...)
• callback 里：如果普通文件：
• 判断是否以 file_extension 结尾（比如 .log）
• 如果匹配：unlink(path) 删除
• 结束可以打印 “deleted X files”。
