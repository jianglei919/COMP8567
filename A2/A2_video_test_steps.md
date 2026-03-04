# A2 视频录制测试步骤（基于当前样例）

## 1. 样例场景信息（原始记录）

### 1.1 启动 `a2sampletree` 输出

```bash
jiang6m@delta:~/workspace/MyProject/COMP8567/A2$ ./a2sampletree
Main Process: PID=1686582, Parent PID=1682584
    Child 1: PID=1686583, Parent PID=1686582
        Grandchild 1 (GC1): PID=1686585, Parent PID=1686583
            Great-Grandchild (GGC): PID=1686589, Parent PID=1686585
        Grandchild 3 (GC3): PID=1686587, Parent PID=1686583
    Child 2: PID=1686584, Parent PID=1686582
        Grandchild 2 (GC2): PID=1686588, Parent PID=1686584
Child 3: PID=1686586, Parent PID=1686582
```

### 1.2 进程查询输出

```bash
jiang6m@delta:~/workspace/MyProject/COMP8567$ ps -u "$USER" -o user,pid,ppid,tty,stat,cmd | awk '$0 ~ /a2sampletree/ {print}'
jiang6m  1686582 1682584 pts/31   SN+  ./a2sampletree
jiang6m  1686583 1686582 pts/31   SN+  ./a2sampletree
jiang6m  1686584 1686582 pts/31   SN+  ./a2sampletree
jiang6m  1686585 1686583 pts/31   SN+  ./a2sampletree
jiang6m  1686586 1686582 pts/31   ZN+  [a2sampletree] <defunct>
jiang6m  1686587 1686583 pts/31   ZN+  [a2sampletree] <defunct>
jiang6m  1686588 1686584 pts/31   ZN+  [a2sampletree] <defunct>
jiang6m  1686589 1686585 pts/31   ZN+  [a2sampletree] <defunct>
jiang6m  1686824 1641817 pts/12   SN+  awk $0 ~ /a2sampletree/ {print}
```

### 1.3 常用排查与清理命令（录制可直接使用）

#### 1）查看当前用户下的进程信息

```bash
ps -u "$USER" -o user,pid,ppid,pgid,%cpu,%mem,vsz,rss,tty,stat,start,time,command
```

#### 2）查看当前用户下 `a2sampletree` 进程信息

```bash
ps -u "$USER" -o user,pid,ppid,tty,stat,cmd -C a2sampletree
ps -u "$USER" -o user,pid,ppid,tty,stat,cmd | awk '$0 ~ /a2sampletree/ {print}'
ps -u "$USER" -o user,pid,ppid,tty,stat,cmd | awk '$0 ~ /[.]\/a2sampletree/ {print}'
```

#### 3）查找当前用户下所有活着的 `a2sampletree`（排除 `Z`）

```bash
ps -o pid,ppid,stat,cmd -C a2sampletree | awk '$3 !~ /^Z/ {print}'
ps -u "$USER" -o pid,ppid,stat,cmd | awk '$3 !~ /^Z/ && $0 ~ /a2sampletree/ {print}'
ps -u "$USER" -o pid,ppid,stat,cmd | awk '$3 !~ /^Z/ && $0 ~ /[.]\/a2sampletree/ {print}'
```

#### 4）终止当前用户下所有非 `Z` 的 `a2sampletree` 进程

```bash
ps -u "$USER" -o pid=,stat=,cmd= | awk '$2 !~ /^Z/ && $0 ~ /[.]\/a2sampletree/ {print $1}' | xargs -r kill -9
```

---

## 2. 视频录制用测试步骤与命令

> 当前样例建议参数：
> - `root_process = 1686582`
> - `process_id = 1686583`

### 2.1 环境准备（开场）

```bash
cd ~/workspace/MyProject/COMP8567/A2
gcc -Wall -Wextra -std=c11 A2_Lei_Jiang.c -o A2
./a2sampletree
ps -u "$USER" -o user,pid,ppid,tty,stat,cmd | awk '$0 ~ /a2sampletree/ {print}'
```

### 2.2 默认功能 + 统计/查询类

```bash
# 默认：属于子树
./A2 1686582 1686583

# 默认：不属于子树（示例：awk 进程）
./A2 1686582 1686824

# 统计/查询
./A2 1686582 1686583 -cnt
./A2 1686582 1686583 -dnd
./A2 1686582 1686583 -oct
./A2 1686582 1686583 -odt
./A2 1686582 1686583 -ndt
./A2 1686582 1686583 -mmd
./A2 1686582 1686583 -mpd
```

### 2.3 控制/信号类（先停再继续）

```bash
# 查看关键进程状态
ps -o pid,ppid,stat,cmd -p 1686583,1686584,1686585,1686586,1686587,1686588,1686589

# 停止兄弟
./A2 1686582 1686583 -sst
ps -o pid,ppid,stat,cmd -p 1686584

# 继续兄弟
./A2 1686582 1686583 -sco
ps -o pid,ppid,stat,cmd -p 1686584
```

### 2.4 kill 类（建议每测一项都重新启动 `a2sampletree`）

```bash
./A2 1686582 1686583 -kpp
./A2 1686582 1686583 -kgp
./A2 1686582 1686583 -ksp
./A2 1686582 1686583 -kps
./A2 1686582 1686583 -kcp
./A2 1686582 1686583 -kgc
./A2 1686582 1686583 -dtm
./A2 1686582 1686583 -krp

# 每次 kill 后都建议查看一次
ps -u "$USER" -o pid,ppid,stat,cmd | awk '$0 ~ /a2sampletree/ {print}'
```

### 2.5 其他类（无 root/process 参数）

```bash
./A2 -bcp
./A2 -bop
```

### 2.6 参数校验（结尾）

```bash
./A2 abc 1686583
./A2 1686582 xyz
./A2 1686582 1686583 -unknown
```

---

## 3. 录制建议（可选）

1. 先录“编译 + 启动样例树 + ps 验证”；
2. 再按“默认/查询 -> 控制信号 -> kill -> 其他 -> 参数校验”顺序展示；
3. kill 类每个选项之间重新运行一次 `./a2sampletree`，避免前一个测试影响后一个结果。
