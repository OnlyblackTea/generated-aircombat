#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <math.h>

// --- 游戏配置参数 ---
#define WIDTH 40        // 游戏区域宽度
#define HEIGHT 25       // 游戏区域高度
#define MAX_BULLETS 100 // 最大子弹数
#define MAX_ENEMIES 10  // 最大敌人数

// --- 数据结构 ---

// 坐标结构 (使用浮点数是为了敌机子弹能计算角度)
typedef struct {
    double x, y;
} Vec2;

// 子弹结构
typedef struct {
    Vec2 pos;
    Vec2 velocity; // 速度向量
    int active;    // 是否激活
    int is_enemy;  // 0: 自机子弹, 1: 敌机子弹
} Bullet;

// 敌机结构
typedef struct {
    Vec2 pos;
    int active;
    int cooldown;  // 发射冷却
} Enemy;

// 自机结构
typedef struct {
    Vec2 pos;
    int lives;
    int score;
    int shoot_timer; // 射击计时器
} Player;

// --- 全局变量 ---
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int frame_count = 0;

// --- 辅助函数：控制台光标 ---

// 隐藏光标，防止闪烁
void HideCursor() {
    CONSOLE_CURSOR_INFO cursor_info = {1, 0};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}

// 移动光标到指定位置 (替代 system("cls"))
void GotoXY(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// --- 游戏逻辑函数 ---

void InitGame() {
    // 初始化玩家
    player.pos.x = WIDTH / 2;
    player.pos.y = HEIGHT - 2;
    player.lives = 3;
    player.score = 0;
    player.shoot_timer = 0;

    // 清空对象池
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = 0;
    for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = 0;
}

// 发射子弹
void SpawnBullet(double x, double y, double vx, double vy, int is_enemy) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].pos.x = x;
            bullets[i].pos.y = y;
            bullets[i].velocity.x = vx;
            bullets[i].velocity.y = vy;
            bullets[i].active = 1;
            bullets[i].is_enemy = is_enemy;
            return;
        }
    }
}

// 生成敌人
void SpawnEnemy() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].pos.x = rand() % (WIDTH - 2) + 1;
            enemies[i].pos.y = 1;
            enemies[i].active = 1;
            enemies[i].cooldown = 20 + rand() % 30; // 随机初始冷却
            return;
        }
    }
}

// 核心更新逻辑
void Update() {
    frame_count++;

    // 1. 玩家移动 (非阻塞输入)
    if (_kbhit()) {
        char key = _getch();
        double speed = 0.7;
        if (key == 'w' && player.pos.y > 1) player.pos.y -= speed;
        if (key == 's' && player.pos.y < HEIGHT - 2) player.pos.y += speed;
        if (key == 'a' && player.pos.x > 1) player.pos.x -= speed;
        if (key == 'd' && player.pos.x < WIDTH - 2) player.pos.x += speed;
    }

    // 2. 玩家自动射击
    // 分数越高，射击间隔越短。最低间隔为 3 帧。
    int fire_rate = 15 - (player.score / 50); 
    if (fire_rate < 3) fire_rate = 3;
    
    player.shoot_timer++;
    if (player.shoot_timer >= fire_rate) {
        SpawnBullet(player.pos.x, player.pos.y - 1, 0, -1.0, 0); // 向上发射
        player.shoot_timer = 0;
    }

    // 3. 更新子弹
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].pos.x += bullets[i].velocity.x;
            bullets[i].pos.y += bullets[i].velocity.y;

            // 边界检查
            if (bullets[i].pos.x <= 0 || bullets[i].pos.x >= WIDTH ||
                bullets[i].pos.y <= 0 || bullets[i].pos.y >= HEIGHT) {
                bullets[i].active = 0;
            }
        }
    }

    // 4. 更新敌人 & 敌机发射自机狙
    if (frame_count % 30 == 0) SpawnEnemy(); // 每30帧尝试生成敌人

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 敌人移动 (缓慢向下)
            enemies[i].pos.y += 0.1; 

            // 消失在底部
            if (enemies[i].pos.y >= HEIGHT - 1) {
                enemies[i].active = 0;
                continue;
            }

            // 发射自机狙 logic
            enemies[i].cooldown--;
            if (enemies[i].cooldown <= 0) {
                // 计算向量
                double dx = player.pos.x - enemies[i].pos.x;
                double dy = player.pos.y - enemies[i].pos.y;
                double dist = sqrt(dx*dx + dy*dy);
                
                // 归一化并设置速度 (子弹速度 0.7)
                if (dist > 0) {
                    double speed = 0.5;
                    SpawnBullet(enemies[i].pos.x, enemies[i].pos.y, (dx/dist)*speed, (dy/dist)*speed, 1);
                }
                enemies[i].cooldown = 40 + rand() % 40; // 重置冷却
            }
        }
    }

    // 5. 碰撞检测
    
    // A. 子弹 vs 敌人
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && !bullets[i].is_enemy) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active) {
                    // 简单的距离判定 (距离小于 1.5 算击中)
                    if (fabs(bullets[i].pos.x - enemies[j].pos.x) < 1.5 && 
                        fabs(bullets[i].pos.y - enemies[j].pos.y) < 1.5) {
                        bullets[i].active = 0;
                        enemies[j].active = 0;
                        player.score += 10;
                    }
                }
            }
        }
    }

    // B. 敌机子弹 vs 玩家
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && bullets[i].is_enemy) {
            if (fabs(bullets[i].pos.x - player.pos.x) < 1.0 && 
                fabs(bullets[i].pos.y - player.pos.y) < 1.0) {
                bullets[i].active = 0;
                player.lives--;
            }
        }
    }

    // C. 敌机本体 vs 玩家 (同归于尽)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
             if (fabs(enemies[i].pos.x - player.pos.x) < 1.5 && 
                 fabs(enemies[i].pos.y - player.pos.y) < 1.5) {
                 enemies[i].active = 0;
                 player.lives = 0; // 直接死亡
             }
        }
    }
}

// 渲染函数 (使用缓冲区思想，直接打印字符)
void Draw() {
    char buffer[HEIGHT][WIDTH + 1];

    // 1. 清空 Buffer (填充背景)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == 0 || y == HEIGHT - 1) buffer[y][x] = '-';
            else if (x == 0 || x == WIDTH - 1) buffer[y][x] = '|';
            else buffer[y][x] = ' ';
        }
        buffer[y][WIDTH] = '\0';
    }

    // 2. 绘制子弹
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            int x = (int)bullets[i].pos.x;
            int y = (int)bullets[i].pos.y;
            if (x > 0 && x < WIDTH - 1 && y > 0 && y < HEIGHT - 1) {
                buffer[y][x] = bullets[i].is_enemy ? '*' : '|';
            }
        }
    }

    // 3. 绘制敌人
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            int x = (int)enemies[i].pos.x;
            int y = (int)enemies[i].pos.y;
            if (x > 0 && x < WIDTH - 1 && y > 0 && y < HEIGHT - 1) {
                buffer[y][x] = 'V';
            }
        }
    }

    // 4. 绘制玩家
    int px = (int)player.pos.x;
    int py = (int)player.pos.y;
    if (px > 0 && px < WIDTH - 1 && py > 0 && py < HEIGHT - 1)
        buffer[py][px] = 'A';

    // 5. 真正的输出到屏幕
    GotoXY(0, 0);
    printf("Score: %d  Lives: %d  (WASD to Move)\n", player.score, player.lives);
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", buffer[y]);
    }
}

int main() {
    srand((unsigned)time(NULL));
    HideCursor();
    InitGame();

    printf("PRESS ANY KEY TO START...");
    _getch();

    while (player.lives > 0) {
        Update();
        Draw();
        Sleep(16); // 控制游戏速度 (~30 FPS)
    }

    GotoXY(WIDTH / 2 - 5, HEIGHT / 2);
    printf("GAME OVER!");
    GotoXY(WIDTH / 2 - 6, HEIGHT / 2 + 1);
    printf("Final Score: %d", player.score);
    
    // 防止程序直接退出
    while(1) if(_kbhit()) break; 
    return 0;
}