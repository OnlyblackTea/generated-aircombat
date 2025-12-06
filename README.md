# GENERATED-AIRCOMBAT

# Describe

这个项目的大部分内容都将由AI生成。

# Context

这是一个非常经典的C语言编程练习。为了让你能直接在Windows环境下编译运行并获得流畅的体验（避免屏幕闪烁），我将使用 \<windows.h> 库来进行光标控制，而不是简单的 system("cls")。

## 代码功能解析

1. 怎么让画面不闪烁？
   在终端游戏中最常见的问题是使用 system("cls") 清屏会导致剧烈闪烁。
    * 解决方案：我使用了 GotoXY(0, 0)。每次循环不清除屏幕，而是把光标移回左上角，然后用新的画面覆盖旧的字符。
    * 双缓冲模拟：在 Draw 函数中，我先在一个二维字符数组 buffer 里画好所有东西，最后一次性 printf 出来，这样视觉上非常稳定。

2. 自机狙 (Enemy Sniper Logic)
   这是你要求的核心难点。敌机需要知道怎么把子弹打向你。
   我在 Update 函数中计算了向量：
   $dx = \text{Player}.x - \text{Enemy}.x$
   $dy = \text{Player}.y - \text{Enemy}.y$
   然后利用勾股定理计算距离 dist。最后归一化向量并乘以速度，得到子弹的 $x$ 和 $y$ 轴分速度。

3. 难度动态调整
   代码中有一行： `int fire_rate = 15 - (player.score / 50);` 这意味着：
    * 初始射击间隔是 15帧（约0.5秒一发）。
    * 每得50分，间隔减小1帧。
    * 这满足了你要求的“分数越高时间间隔越短”。

4. 碰撞判定
因为终端是格子的，但我们的坐标是 `double` 类型的浮点数（为了平滑移动），所以判断碰撞时我用了 `fabs` (绝对值) 来判断物体中心点是否接近： `fabs(bullet.x - enemy.x) < 1.5` 这比简单的 `(int)x == (int)x` 判定要更加宽容和准确。

# How to run
1. 环境：Windows 操作系统。

2. 编译器：GCC (MinGW), Clang, 或者 Visual Studio 的 MSVC 均可。

3. 编译命令 (如果你用 GCC):
    ```Bash
    gcc plane_game.c -o plane_game.exe
    ./plane_game.exe
    ```

4. 操作：
    * W, A, S, D 控制移动。
    * 不需要按键射击，飞机会自动开火。