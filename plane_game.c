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
#define MAX_ITEMS 5     // 最大道具数
#define MAX_EXPLOSIONS 10 // 最大爆炸效果数

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
    int type;      // 敌机类型: 0=普通(自机狙), 1=直线机, 2=散射机
} Enemy;

// 道具结构
typedef struct {
    Vec2 pos;
    int active;
    int type;      // 0=生命恢复(H), 1=火力升级(P)
} Item;

// 爆炸效果结构
typedef struct {
    Vec2 pos;
    int active;
    int timer;     // 爆炸持续时间
} Explosion;

// 自机结构
typedef struct {
    Vec2 pos;
    int lives;
    int score;
    int shoot_timer; // 射击计时器
    int power_level; // 火力等级: 0=普通, 1=双发, 2=三发
    int power_timer; // 火力升级持续时间
    int slow_mode;   // 慢速移动模式: 0=正常, 1=精确模式
} Player;

// --- 全局变量 ---
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
Item items[MAX_ITEMS];
Explosion explosions[MAX_EXPLOSIONS];
int frame_count = 0;
int high_score = 0; // 最高分记录

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

// --- 音效函数 ---

// 播放射击音效 (高音)
void PlayShootSound() {
    Beep(1200, 30); // 1200Hz, 30ms
}

// 播放击中音效 (中音)
void PlayHitSound() {
    Beep(800, 50); // 800Hz, 50ms
}

// 播放被击中音效 (低音)
void PlayDamageSound() {
    Beep(300, 100); // 300Hz, 100ms
}

// 播放爆炸音效 (报警音)
void PlayExplosionSound() {
    Beep(200, 150); // 200Hz, 150ms
}

// --- 最高分管理函数 ---

// 从文件读取最高分
void LoadHighScore() {
    FILE* file = fopen("highscore.txt", "r");
    if (file != NULL) {
        fscanf(file, "%d", &high_score);
        fclose(file);
    }
}

// 保存最高分到文件
void SaveHighScore() {
    if (player.score > high_score) {
        high_score = player.score;
        FILE* file = fopen("highscore.txt", "w");
        if (file != NULL) {
            fprintf(file, "%d", high_score);
            fclose(file);
        }
    }
}

// --- 游戏逻辑函数 ---

void InitGame() {
    // 初始化玩家
    player.pos.x = WIDTH / 2;
    player.pos.y = HEIGHT - 2;
    player.lives = 3;
    player.score = 0;
    player.shoot_timer = 0;
    player.power_level = 0;
    player.power_timer = 0;
    player.slow_mode = 0;

    // 清空对象池
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = 0;
    for(int i=0; i<MAX_ENEMIES; i++) enemies[i].active = 0;
    for(int i=0; i<MAX_ITEMS; i++) items[i].active = 0;
    for(int i=0; i<MAX_EXPLOSIONS; i++) explosions[i].active = 0;
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

// 生成敌人 - 根据分数决定类型
void SpawnEnemy() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].pos.x = rand() % (WIDTH - 2) + 1;
            enemies[i].pos.y = 1;
            enemies[i].active = 1;
            enemies[i].cooldown = 20 + rand() % 30; // 随机初始冷却
            
            // 根据分数决定敌机类型
            if (player.score < 100) {
                enemies[i].type = 0; // 只有普通敌机
            } else if (player.score < 300) {
                enemies[i].type = (rand() % 100 < 70) ? 0 : 1; // 70%普通, 30%直线
            } else {
                int r = rand() % 100;
                if (r < 50) enemies[i].type = 0;      // 50%普通
                else if (r < 80) enemies[i].type = 1; // 30%直线
                else enemies[i].type = 2;             // 20%散射
            }
            return;
        }
    }
}

// 生成道具
void SpawnItem(double x, double y, int type) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            items[i].pos.x = x;
            items[i].pos.y = y;
            items[i].active = 1;
            items[i].type = type;
            return;
        }
    }
}

// 生成爆炸效果
void SpawnExplosion(double x, double y) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i].pos.x = x;
            explosions[i].pos.y = y;
            explosions[i].active = 1;
            explosions[i].timer = 10; // 爆炸持续10帧
            return;
        }
    }
}

// 核心更新逻辑
void Update() {
    frame_count++;

    // 1. 玩家移动 (非阻塞输入)
    // 检测Shift键状态 (使用GetAsyncKeyState)
    player.slow_mode = (GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);
    
    if (_kbhit()) {
        char key = _getch();
        
        // 根据模式设置移动速度
        double speed = player.slow_mode ? 0.25 : 0.7; // 慢速模式为1/3-1/4速度
        
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
        // 根据火力等级发射子弹
        if (player.power_level == 0) {
            SpawnBullet(player.pos.x, player.pos.y - 1, 0, -1.0, 0);
        } else if (player.power_level == 1) {
            // 双发
            SpawnBullet(player.pos.x - 0.5, player.pos.y - 1, 0, -1.0, 0);
            SpawnBullet(player.pos.x + 0.5, player.pos.y - 1, 0, -1.0, 0);
        } else {
            // 三发
            SpawnBullet(player.pos.x - 0.7, player.pos.y - 1, 0, -1.0, 0);
            SpawnBullet(player.pos.x, player.pos.y - 1, 0, -1.0, 0);
            SpawnBullet(player.pos.x + 0.7, player.pos.y - 1, 0, -1.0, 0);
        }
        PlayShootSound(); // 播放射击音效
        player.shoot_timer = 0;
    }
    
    // 更新火力升级计时器
    if (player.power_timer > 0) {
        player.power_timer--;
        if (player.power_timer == 0) {
            player.power_level = 0; // 恢复普通火力
        }
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

    // 4. 更新敌人 & 敌机发射
    // 分数越高，生成频率越高
    int spawn_interval = 30 - (player.score / 100);
    if (spawn_interval < 15) spawn_interval = 15;
    if (frame_count % spawn_interval == 0) SpawnEnemy();

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            // 根据类型移动
            if (enemies[i].type == 0) {
                // 普通敌机：缓慢向下
                enemies[i].pos.y += 0.1;
            } else if (enemies[i].type == 1) {
                // 直线机：快速向下
                enemies[i].pos.y += 0.3;
            } else {
                // 散射机：缓慢向下
                enemies[i].pos.y += 0.08;
            }

            // 消失在底部
            if (enemies[i].pos.y >= HEIGHT - 1) {
                enemies[i].active = 0;
                continue;
            }

            // 发射子弹逻辑
            enemies[i].cooldown--;
            if (enemies[i].cooldown <= 0) {
                if (enemies[i].type == 0) {
                    // 普通敌机：发射自机狙
                    double dx = player.pos.x - enemies[i].pos.x;
                    double dy = player.pos.y - enemies[i].pos.y;
                    double dist = sqrt(dx*dx + dy*dy);
                    
                    if (dist > 0) {
                        double speed = 0.5;
                        SpawnBullet(enemies[i].pos.x, enemies[i].pos.y, (dx/dist)*speed, (dy/dist)*speed, 1);
                    }
                    enemies[i].cooldown = 40 + rand() % 40;
                } else if (enemies[i].type == 2) {
                    // 散射机：发射三发散射弹
                    SpawnBullet(enemies[i].pos.x, enemies[i].pos.y, -0.3, 0.5, 1); // 左下
                    SpawnBullet(enemies[i].pos.x, enemies[i].pos.y, 0, 0.6, 1);    // 正下
                    SpawnBullet(enemies[i].pos.x, enemies[i].pos.y, 0.3, 0.5, 1);  // 右下
                    enemies[i].cooldown = 50 + rand() % 30;
                }
                // 直线机不发射子弹
            }
        }
    }
    
    // 5. 更新道具
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            items[i].pos.y += 0.15; // 缓慢下落
            
            // 消失在底部
            if (items[i].pos.y >= HEIGHT - 1) {
                items[i].active = 0;
            }
        }
    }
    
    // 6. 更新爆炸效果
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            explosions[i].timer--;
            if (explosions[i].timer <= 0) {
                explosions[i].active = 0;
            }
        }
    }

    // 7. 碰撞检测 (优化判定精度)
    
    // A. 子弹 vs 敌人
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && !bullets[i].is_enemy) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active) {
                    // 优化判定：子弹判定为 0.8
                    if (fabs(bullets[i].pos.x - enemies[j].pos.x) < 0.8 && 
                        fabs(bullets[i].pos.y - enemies[j].pos.y) < 0.8) {
                        bullets[i].active = 0;
                        enemies[j].active = 0;
                        
                        // 生成爆炸效果
                        SpawnExplosion(enemies[j].pos.x, enemies[j].pos.y);
                        PlayHitSound(); // 播放击中音效
                        
                        // 根据敌机类型给予不同分数
                        if (enemies[j].type == 0) player.score += 10;
                        else if (enemies[j].type == 1) player.score += 15;
                        else player.score += 20;
                        
                        // 10%概率掉落道具
                        int drop_rand = rand();
                        if (drop_rand % 100 < 10) {
                            int item_type = (drop_rand / 100) % 2; // 0=生命, 1=火力
                            SpawnItem(enemies[j].pos.x, enemies[j].pos.y, item_type);
                        }
                    }
                }
            }
        }
    }

    // B. 敌机子弹 vs 玩家 (缩小判定到 0.5，实现擦弹)
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && bullets[i].is_enemy) {
            if (fabs(bullets[i].pos.x - player.pos.x) < 0.5 && 
                fabs(bullets[i].pos.y - player.pos.y) < 0.5) {
                bullets[i].active = 0;
                player.lives--;
                PlayDamageSound(); // 播放被击中音效
            }
        }
    }

    // C. 敌机本体 vs 玩家 (缩小判定)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
             if (fabs(enemies[i].pos.x - player.pos.x) < 0.8 && 
                 fabs(enemies[i].pos.y - player.pos.y) < 0.8) {
                 enemies[i].active = 0;
                 SpawnExplosion(enemies[i].pos.x, enemies[i].pos.y);
                 PlayExplosionSound(); // 播放爆炸音效
                 player.lives = 0; // 直接死亡
             }
        }
    }
    
    // D. 玩家 vs 道具
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            if (fabs(items[i].pos.x - player.pos.x) < 1.2 && 
                fabs(items[i].pos.y - player.pos.y) < 1.2) {
                items[i].active = 0;
                
                if (items[i].type == 0) {
                    // 生命恢复
                    if (player.lives < 5) player.lives++;
                } else {
                    // 火力升级
                    if (player.power_level < 2) player.power_level++;
                    player.power_timer = 625; // 10秒 (625帧 @ 62.5fps with Sleep(16))
                }
            }
        }
    }
}

// 辅助函数：在buffer中安全地放置字符
void PutChar(char buffer[HEIGHT][WIDTH + 1], int x, int y, char c) {
    if (x > 0 && x < WIDTH - 1 && y > 0 && y < HEIGHT - 1) {
        buffer[y][x] = c;
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
            PutChar(buffer, x, y, bullets[i].is_enemy ? '*' : '|');
        }
    }

    // 3. 绘制道具
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            int x = (int)items[i].pos.x;
            int y = (int)items[i].pos.y;
            char icon = (items[i].type == 0) ? 'H' : 'P';
            PutChar(buffer, x, y, icon);
        }
    }

    // 4. 绘制爆炸效果 (多字符)
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
            int x = (int)explosions[i].pos.x;
            int y = (int)explosions[i].pos.y;
            
            // 根据计时器显示不同阶段的爆炸
            if (explosions[i].timer > 6) {
                PutChar(buffer, x, y, '#');
                PutChar(buffer, x-1, y, '*');
                PutChar(buffer, x+1, y, '*');
            } else if (explosions[i].timer > 3) {
                PutChar(buffer, x, y, 'X');
                PutChar(buffer, x-1, y, 'x');
                PutChar(buffer, x+1, y, 'x');
            } else {
                PutChar(buffer, x, y, '+');
            }
        }
    }

    // 5. 绘制敌人 (多字符造型)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            int x = (int)enemies[i].pos.x;
            int y = (int)enemies[i].pos.y;
            
            if (enemies[i].type == 0) {
                // 普通敌机 - 使用V字型
                PutChar(buffer, x, y, 'V');
                PutChar(buffer, x-1, y-1, '\\');
                PutChar(buffer, x+1, y-1, '/');
            } else if (enemies[i].type == 1) {
                // 直线机 - 使用简单三角
                PutChar(buffer, x, y, 'v');
                PutChar(buffer, x, y-1, '|');
            } else {
                // 散射机 - 使用W字型
                PutChar(buffer, x, y, 'W');
                PutChar(buffer, x-1, y-1, '<');
                PutChar(buffer, x+1, y-1, '>');
            }
        }
    }

    // 6. 绘制玩家 (多字符造型)
    int px = (int)player.pos.x;
    int py = (int)player.pos.y;
    if (px > 0 && px < WIDTH - 1 && py > 0 && py < HEIGHT - 1) {
        PutChar(buffer, px, py, 'A');
        PutChar(buffer, px-1, py+1, '/');
        PutChar(buffer, px+1, py+1, '\\');
        PutChar(buffer, px, py-1, '^');
        
        // 在慢速模式下显示精确判定点
        if (player.slow_mode) {
            PutChar(buffer, px, py, 'o'); // 显示判定点
        }
    }

    // 7. 真正的输出到屏幕
    GotoXY(0, 0);
    
    // 生命值条显示
    printf("Score: %d  Lives: ", player.score);
    for (int i = 0; i < player.lives && i < 5; i++) {
        printf("*");
    }
    for (int i = player.lives; i < 5; i++) {
        printf("-");
    }
    
    // 火力等级显示
    if (player.power_level > 0) {
        printf("  POWER: ");
        for (int i = 0; i < player.power_level; i++) {
            printf("P");
        }
        printf(" (%ds)", (player.power_timer * 16) / 1000); // 正确计算秒数
    }
    
    // 慢速模式显示
    if (player.slow_mode) {
        printf("  [SLOW]");
    }
    
    printf("  (WASD + Space/Shift)\n");
    
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", buffer[y]);
    }
}

int main() {
    srand((unsigned)time(NULL));
    HideCursor();
    LoadHighScore(); // 加载最高分
    InitGame();

    printf("HIGH SCORE: %d\n", high_score);
    printf("PRESS ANY KEY TO START...");
    _getch();

    while (player.lives > 0) {
        Update();
        Draw();
        Sleep(16); // 控制游戏速度 (~62.5 FPS)
    }

    // 检查是否破纪录
    int is_new_record = (player.score > high_score);
    
    // 保存最高分
    SaveHighScore();
    
    GotoXY(WIDTH / 2 - 5, HEIGHT / 2);
    printf("GAME OVER!");
    GotoXY(WIDTH / 2 - 6, HEIGHT / 2 + 1);
    printf("Final Score: %d", player.score);
    GotoXY(WIDTH / 2 - 6, HEIGHT / 2 + 2);
    
    // 显示最高分信息
    if (is_new_record) {
        printf("NEW HIGH SCORE!");
    } else {
        printf("High Score: %d", high_score);
    }
    
    // 防止程序直接退出
    while(1) if(_kbhit()) break; 
    return 0;
}