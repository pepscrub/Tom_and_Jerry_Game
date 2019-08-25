#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <cab202_graphics.h>
#include <cab202_timers.h>


#include <stdio.h>
#include <stdbool.h>

#define MAX_ITEMS (100)
#define BUFFER 50
#define SPEED 1

#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define ABS(x)	 (((x) >= 0) ? (x) : -(x))

struct game_logic
{
    bool g_over;
    int score;
    int loop_delay;
    
    int c_active;
    int c_interval;
    int c_advance;

    int mt_active;
    int mt_max;
    int mt_interval;

    int fw_active;
    int fw_max;
    int fw_interval;

    int lives;
    int time;
    int level;

    int W;
    int H;

    // Status bar out of bounds collision
    int ob_x;
    int ob_y;

    bool wall;
} game;

// Setting up character variable here acts like a global var
struct character
{
    double speed;
    double x,y;
    char img;
    int health;
} jerry, tom;

// Global variables
double walls[50][4];
double walls_scaled[50][4];
double wall_pixels[1000][2]; // Used to store coords of known wall pixels 


void scale_walls()
{
    for(int i = 0; i != 50; i++)
    {
        if(walls[i][0] * game.W != 0)
        {
            walls_scaled[i][0] = walls[i][0] * game.W;
            walls_scaled[i][1] = (walls[i][1] * (game.H - game.ob_y)) + game.ob_y;
            walls_scaled[i][2] = walls[i][2] * game.W;
            walls_scaled[i][3] = (walls[i][3] * (game.H - game.ob_y)) + game.ob_y;
        }
    }
}



void status_bar()
{
    double margin = (game.W - 9.5) / 4;
    draw_formatted(0,0,"Student Number: 10407642");
    draw_formatted(margin,0,"Score: %d", game.score);
    draw_formatted(margin*2,0,"Lives: %d", game.lives);
    draw_formatted(margin*3,0,"Player: %c", jerry.img);
    draw_formatted(margin*4,0,"Time 00:00", game.time);
    // Second bar
    draw_formatted(0,1,"Cheese: %d", game.c_active);
    draw_formatted(margin,1,"Traps: %d", game.mt_active);
    draw_formatted(margin*2,1,"Fireworks: %d", game.fw_active);
    draw_formatted(margin*3,1,"Level: %d", game.level);
    draw_line(0, 2, game.W, 2, '-');
}

void calc_wall_pixels()
{
    int pixel_count = 0;
    for(int l = 0; l != 50; l++)
    {
        int x1 = walls_scaled[l][0];
        int y1 = walls_scaled[l][1];
        int x2 = walls_scaled[l][2];
        int y2 = walls_scaled[l][3];

        wall_pixels[pixel_count][0] = 0; // x
        wall_pixels[pixel_count][1] = 0; // y

        if(x1 == x2)
        {
            int y_min = MIN(y1, y2),y_max = MAX(y1, y2);
            for(int i = y_min; i <= y_max; i++)
            {
                wall_pixels[pixel_count][0] = x1;
                wall_pixels[pixel_count][1] = i;
                pixel_count++;
            }
        }
        else if(y1 == y2)
        {
            int x_min = MIN(x1, x2),x_max = MAX(x1, x2);
            for(int i = x_min; i <= x_max; i++)
            {
                wall_pixels[pixel_count][0] = i;
                wall_pixels[pixel_count][1] = y1;
                pixel_count++;
            }
        }
        else
        {
            if (x1 > x2) {
                int t = x1;
                x1 = x2;
                x2 = t;
                t = y1;
                y1 = y2;
                y2 = t;
            }

            // Use the Bresenham algorithm to render the line.
            // TODO: Convert to an integer-only implementation such as
            // https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
            float dx = x2 - x1;
            float dy = y2 - y1;
            float err = 0.0;
            float derr = ABS(dy / dx);

            for (int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x--) {
                wall_pixels[pixel_count][0] = x;
                wall_pixels[pixel_count][1] = y;
                err += derr;
                pixel_count++;
                while (err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2)) {
                    wall_pixels[pixel_count][0] = x;
                    wall_pixels[pixel_count][1] = y;
                    y += (dy > 0) - (dy < 0);
                    err -= 1.0;
                    pixel_count++;
                }
            }
        }
    }
}

void collision_wall( char key )
{
    calc_wall_pixels();
    for(int i = 0; i != 5000; i++)
    {
        if(wall_pixels[i][0] != 0)
        {
            if(key == 'a' && jerry.x - 1 == round(wall_pixels[i][0]) && wall_pixels[i][1] == jerry.y) jerry.x++;
            if(key == 'd' && jerry.x + 1 == round(wall_pixels[i][0]) && wall_pixels[i][1] == jerry.y) jerry.x--;
            if(key == 'w' && jerry.y - 1 == round(wall_pixels[i][1]) && wall_pixels[i][0] == jerry.x) jerry.y++;
            if(key == 's' && jerry.y + 1 == round(wall_pixels[i][1]) && wall_pixels[i][0] == jerry.x) jerry.y--;
        }
    }
}

void draw_wall(void)
{
    scale_walls();
    for(int i = 0; i != 50; i++)
    {
        if(walls_scaled[i][0] != 0)
        {
            draw_line(walls_scaled[i][0], walls_scaled[i][1], walls_scaled[i][2], walls_scaled[i][3], '*');
        }
    }
}


void read_file(FILE*stream)
{
    int wall_num=0;
    while(!feof(stream))
    {
        char command;
        double a1, a2, a3, a4;
        double num = fscanf(stream, " %c %lf %lf %lf %lf", &command, &a1, &a2, &a3, &a4);
        if(num == 3) // Number
        {
            if(command == 'J')
            {
                jerry.img = command;
                jerry.x = a1;
                jerry.y = a2;
            }
            if(command == 'T')
            {
                tom.img = command;
                tom.x = a1;
                tom.y = a2;
            }
        }
        else if(num == 5)
        {
            if(command == 'W')
            {
                walls[wall_num][0] = a1;
                walls[wall_num][1] = a2;
                walls[wall_num][2] = a3;
                walls[wall_num][3] = a4;
                wall_num++;
            }
        }
    }
}

void jerry_setup() 
{
    jerry.speed = 1;
    jerry.health = 100;

    jerry.x = jerry.x * game.W;
    jerry.y = (jerry.y * game.H) + game.ob_y;

}

void tom_setup()
{
    tom.speed = jerry.speed / 2;
    tom.health = 100;

    tom.x = tom.x * game.W;
    tom.y = (tom.y * game.H) + game.ob_y;
}

void tom_ob_check()
{
    if(tom.x < game.ob_x){tom.x++;return;}
    if(tom.y < game.ob_y){tom.y++;return;}
    if(tom.x > game.W) {tom.x--;return;} 
    if(tom.y > game.H) {tom.y--;return;}
}

void tom_ai()
{
    // Use double and round in checks
    double dy = tom.y - jerry.y;
    double dx = tom.x - jerry.x;

    draw_int(10, 24, dx);
    draw_int(10, 25, dy);

    if(round(dy) > 0 && tom.y != 0) tom.y = tom.y - (tom.speed / 3);
    else if(dy < 0 && tom.y != 0) tom.y = tom.y + (tom.speed / 3);
    else if(round(dx) > 0 && tom.x != 0) tom.x = tom.x - (tom.speed / 3);
    else if(dx < 0 && tom.x != 0) tom.x = tom.x + (tom.speed / 3);
}

void controller( void )
{
    int key = get_char();
    if(game.wall) return;

    collision_wall( key );

    if(key == 'a' && jerry.x - 1 >= game.ob_x) jerry.x--;
    else if (key == 'd' && jerry.x + 1 < game.W) jerry.x++;
    else if (key == 'w' && jerry.y - 1 >= game.ob_y) jerry.y--;
    else if (key == 's' && jerry.y + 1 < game.H) jerry.y++;
}

void game_logic_setup (void)
{   
    // Game conditions
    game.g_over = false;
    game.score = 0;
    game.loop_delay = 10;

    game.ob_x = 0;
    game.ob_y = 3;


    // Cheese
    game.c_active = 0;
    game.c_advance = 5;
    game.c_interval = 2;

    // Mouse traps
    game.mt_active = 0;
    game.mt_interval = 3;
    game.mt_max = 5;

    game.fw_active = 0;
    game.fw_interval = 0;
    game.fw_max = 0;

    game.lives = 5;
    game.time = 0;

    game.level = 0;

    // Screen
    game.W = screen_width();
    game.H = screen_height();
    game.wall = false; // Collision boolean
}

void setup (void) {
    game_logic_setup();
    jerry_setup();
    tom_setup();
    status_bar();
}


void debugging()
{
    draw_int(10, 10, jerry.x);
    draw_int(10, 11, jerry.y);

    draw_int(10, 13, tom.x);
    draw_int(10, 14, tom.y);
}

void loop(void) 
{
    clear_screen();
    game.W = screen_width();
    game.H = screen_height();

    debugging();

    status_bar();
    draw_wall();
    controller();
    tom_ob_check();
    // tom_ai();


    draw_char(jerry.x, jerry.y, jerry.img);
    draw_char(tom.x, tom.y, tom.img);

    show_screen();
}

int main(int argc, char *argv[]) 
{
    for (int i = 1; i < argc; i++) 
    {
        FILE * stream = fopen(argv[i], "r");
        if (stream != NULL)
        {
            read_file(stream);
            fclose(stream);
        }
    }
    setup_screen();
    setup();

    draw_wall();
    show_screen();

    while ( !game.g_over ) {
        loop();
        timer_pause(10);
    }
    return 0;
}