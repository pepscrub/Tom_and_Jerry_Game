#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <cab202_graphics.h>
#include <cab202_timers.h>


#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define MAX_ITEMS (100)
#define BUFFER 50;
#define SPEED 1; // Default speed (directly affects BOTH Tom and Jerrys movement speed as well as Toms wall bounce)
#define REFRESHRATE 5

#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define ABS(x)	 (((x) >= 0) ? (x) : -(x))
#define TPOSCALC(c,s,t) (t == 0 ? ( game.charswitch == 0 ? c + (s / REFRESHRATE) : c + s ) : ( game.charswitch == 0 ? c - (s / REFRESHRATE) : c - s ) ) // C = current position, S = speed, T = Type (Type refers to either positive or negative of Jerrys coords)
#define SCALEX(x) (x * game.W) // Scale wall point to screen resolution
#define SCALEY(y) ((y * (game.H - game.ob_y)) + game.ob_y) // Scale wall point to resolution on Y (includes automatic scaling from the status bar)
#define CRENDER(x, y, i) (x != 0 && y != 0 ? draw_char(x, y, i) : 0)

/*
    Quick-start command:
    gcc -g tomandjerry.c -o tomandjerry -Wall -Werror -L../ZDK -I../ZDK -lzdk -lm -lncurses;./tomandjerry.exe room00.txt 
*/



/*
    Contains the vast majority of game mechanics and logic.
*/
struct game_logic
{
    bool g_over; // Game over / reset
    bool pause; // Game pause mode
    int score; // Current game score
    int loop_delay; // Game rendering delay

    int c_active; // Cheese active elements
    int c_interval; // Cheese interval for placing cheese
    int c_advance; // Cheese max before next room loads 

    int mt_active;
    int mt_max;
    int mt_interval;

    int fw_active;
    int fw_max;
    int fw_interval;

    int lives; // Lives remaining
    int min; // Timer min
    int sec; // Timer sec
    int cpsec; // Previous second for cheese
    int mtpsec;
    int milsec; // Timer milliseconds
    int currlvl; // Current level
    int maxlvls; // Max levels
    char level[10][12]; // levels

    int drawn_door;
    int advance;
    int firework;

    int W;
    int H;
    char player;

    int charswitch;

    // Status bar out of bounds collision
    int ob_x;
    int ob_y;
    int exit;
} game;

/*
    Game objects that require X and Y interacitivty 
*/
struct gObj
{
    int x;
    int y;
} c1, c2, c3, c4 ,c5, t1, t2 ,t3 ,t4, t5, door;

/*
    Contains struct for both jerry and toms x,y speed and global vars that need
    ready access.
*/
struct character
{
    double speed;
    double x,y;
    int start_x,start_y;
    char img;
    int wall;
} jerry, tom, fw;




// Globals
double walls[50][4]; // Walls initial position
double walls_scaled[50][4]; // Walls scaled up to screen resolution
double dx,dy; // Delta X and Delta Y 

/*
    Generic function that scales the walls up to the screen resoultion
*/
void scale_walls()
{
    for(int i = 0; i != 50; i++)
    {
        if(walls[i][0] * game.W != 0)
        {
            walls_scaled[i][0] = SCALEX(walls[i][0]);
            walls_scaled[i][1] = SCALEY(walls[i][1]);
            walls_scaled[i][2] = SCALEX(walls[i][2]);
            walls_scaled[i][3] = SCALEY(walls[i][3]);
        }
    }
}

/* 
    Reset dynamic game mechanics
*/
void reset_gameObj( void )
{
    c1.x = 0, c1.y = 0;
    c2.x = 0, c2.y = 0;
    c3.x = 0, c3.y = 0;
    c4.x = 0, c4.y = 0;
    c5.x = 0, c5.y = 0;
    t1.x = 0, t1.y = 0;
    t2.x = 0, t2.y = 0;
    t3.x = 0, c3.y = 0;
    t4.x = 0, t4.y = 0;
    t5.x = 0, t5.y = 0;

    game.mt_active = 0;
    game.c_active = 0;

    door.x = -1;
    door.y = -1;
}
/*
    Reset core game mechanics 
*/
void game_reset( void )
{
   // Game conditions
    game.g_over = false;
    game.score = 0;

    game.drawn_door = 0;

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

    fw.x = 0, fw.y = 0, game.fw_active = 0;

    game.lives = 5;

    reset_gameObj();
}

/*
    Reset Jerry and Tom to their starting positions
*/
void reset()
{
    game_reset();
    jerry.x = jerry.start_x;
    jerry.y = jerry.start_y;
    tom.x = tom.start_x;
    tom.y = tom.start_y;
    game.advance = 0;
}

void status_bar()
{
    // * Surely there's a cleaner way to do this?
    double margin = (game.W - 9.5) / 4;
    draw_formatted(0,0,"Student Number: 10407642");
    draw_formatted(margin,0,"Score: %d", game.score);
    draw_formatted(margin*2,0,"Lives: %d", game.lives);
    draw_formatted(margin*3,0,"Player: %c", game.player);
    draw_formatted(margin*4,0,"Time %02d:%02d", game.min, game.sec);
    draw_formatted(0,1,"Cheese: %d", game.c_active);
    draw_formatted(margin,1,"Traps: %d", game.mt_active);
    draw_formatted(margin*2,1,"Fireworks: %d", game.fw_active);
    draw_formatted(margin*3,1,"Level: %d", game.currlvl);
    draw_line(0, 2, game.W, 2, '-');
}

/*
    Checks to see if a wall is 1 pixel away from tom in all directions
    returns: 1 (Wall detected)
    returns: 0 (No Wall detected)
*/
int check_wall ( void )
{
    int t = 0;
    if(scrape_char(round(tom.x - 1), round(tom.y)) == '*')          tom.x = round(tom.x + 1), t = 1; // left 
    if(scrape_char(round(tom.x + 1), round(tom.y)) == '*')          tom.x = round(tom.x - 1), t = 1; // right
    if(scrape_char(round(tom.x), round(tom.y - 1)) == '*')          tom.y = round(tom.y + 1), t = 1; // below
    if(scrape_char(round(tom.x), round(tom.y + 1)) == '*')          tom.y = round(tom.y - 1), t = 1; // above
    return t;
}

bool check_pixel(int x, int y)
{
    if(scrape_char(x, y) == '*') return true;
    else return false;
}

/*
    Calculates Tom seeking path and wall collision randomization
    Called from Wall Collision loop
*/
void tom_ai( void )
{
    draw_double(10, 10, tom.x);
    draw_double(10, 11, tom.y);
    int near_wall = check_wall();
    if( game.pause ) return;
    if(floor(tom.x) == floor(jerry.x) && floor(tom.y) == floor(jerry.y)) game.lives--, reset();
    if(!near_wall)
    {
        if(scrape_char(round(tom.x), round(tom.y) + 1) == '*') tom.y = round(tom.y - 1);
        if(!jerry.wall)
        {
            int dx,dy;
            dx = jerry.x - tom.x;
            dy = jerry.y - tom.y;

            if( round(jerry.x) == round(tom.x) && round(tom.y) <= round(jerry.y) )           tom.y = TPOSCALC(round(tom.y), tom.speed, 1),near_wall = 0;
            else if( round(jerry.x) == round(tom.x) && round(tom.y) > round(jerry.y)  )      tom.y = TPOSCALC(round(tom.y), tom.speed, 0),near_wall = 0;
            else if( round(jerry.y) == round(tom.y) && round(tom.x) <= round(jerry.x) )      tom.x = TPOSCALC(round(tom.x), tom.speed, 1),near_wall = 0;
            else if( round(jerry.y) == round(tom.y) && round(tom.x) >= round(jerry.x)  )      tom.x = TPOSCALC(round(tom.x), tom.speed, 0),near_wall = 0;
            else // Line algroithim appeared to exibit the same behaviours as the code below
            {
                if ( dx < 0 ) tom.x = TPOSCALC(tom.x, tom.speed, 1),near_wall = 0;
                if ( dx > 0 ) tom.x = TPOSCALC(tom.x, tom.speed, 0),near_wall = 0;
                if ( dy < 0 ) tom.y = TPOSCALC(tom.y, tom.speed, 1),near_wall = 0;
                if ( dy > 0 ) tom.y = TPOSCALC(tom.y, tom.speed, 0),near_wall = 0;
            }

        }
    }
    else
    {
            int js,ts,x,y;
            js = jerry.speed;
            // Tom speed needs to be rounded up to 1 so we can use rand()
            ts = tom.speed <= 1 ? 1 : tom.speed;
            // Fly window interaction
            // x and y check to see if Jerry is left/right above/below the players current coordinate
            // TODO: Add minmum distance based on Toms position and the nearest wall behind the teleport
            // * Could also animate this process very easily
            x = tom.x > jerry.x ? tom.x + rand() % 2 : tom.x - rand() % 2;
            y = tom.y > jerry.y ? tom.y + rand() % 2 : tom.y - rand() % 2;

            tom.x = TPOSCALC(x, rand() % (ts + js), (tom.x < jerry.x ? 0 : 1));
            tom.y = TPOSCALC(y, rand() % (ts + js), (tom.y < jerry.y ? 0 : 1));
            tom.wall = 0;
    }
}

void firework()
{
    fw.x = round(jerry.x) == 0 ? round(jerry.x) + 1 : round(jerry.x);
    fw.y = round(jerry.y);
}

int collect_cheese = 0;

double tmp_x,tmp_y;

void jerry_ai( void )
{
    if(floor(tom.x) == floor(jerry.x) && floor(tom.y) == floor(jerry.y)) game.score += 5, reset();
    if( game.pause ) return;
    /*
        * Failsafe check
        Jerry showed unexepected behaviours when there were no cheese present on screen, after collecting his first and there was no secondary peice.
        Jerrys X and Y would become nan. This was the simplist solution due to it occuring in a 1-2 second period and wouldn't interfere if the majority
        of games
    */
    if( game.c_active == 0) return; 


    double tcx = 0;
    double tcy = 0;

    if(tcx != 0 && tcy != 0) collect_cheese = 1;

    // Seeking cheese mechanic (Checks to see which cheese is spawned)
    tcx = c1.x != 0 ? c1.x - jerry.x : (c2.x != 0 ? c2.x - jerry.x : (c3.x != 0 ? c3.x - jerry.x : (c4.x != 0 ? c4.x - jerry.x : (c5.x != 0 ? c5.x - jerry.x : 0))));
    tcy = c1.y != 0 ? c1.y - jerry.y : (c2.y != 0 ? c2.y - jerry.y : (c3.y != 0 ? c3.y - jerry.y : (c4.y != 0 ? c4.y - jerry.x : (c5.y != 0 ? c5.y - jerry.y : 0))));

    if(c1.x - jerry.x < c2.x - jerry.x && c1.y - jerry.y < c2.x - jerry.y) tcx = c2.x - jerry.x, tcy = c2.y - jerry.y;
    else if(c2.x - jerry.x < c3.x - jerry.x && c2.y - jerry.y < c3.x - jerry.y) tcx = c3.x - jerry.x, tcy = c3.y - jerry.y;
    else if(c3.x - jerry.x < c4.x - jerry.x && c3.y - jerry.y < c4.x - jerry.y) tcx = c4.x - jerry.x, tcy = c4.y - jerry.y;
    else if(c4.x - jerry.x < c5.x - jerry.x && c4.y - jerry.y < c5.x - jerry.y) tcx = c5.x - jerry.x, tcy = c5.y - jerry.y;

    if(tcx != 0 && tcy != 0) collect_cheese = 1;

    double d,dx,dy;

    d = sqrt(tcx*tcx + tcy* tcy);

    dx = (tcx * .15 / d);
    dy = (tcy * .15 / d);  

    // Collision

    int jx,jy,tx,ty;
    jx = round(jerry.x);
    jy = round(jerry.y);
    tx = round(tom.x);
    ty = round(tom.y);

    if(scrape_char(jx + 1, jy) == '*') jerry.x = round(jerry.x - 1);
    else if(scrape_char(jx - 1, jy) == '*') jerry.x = round(jerry.x + 1);
    else if(scrape_char(jx, jy+ 1) == '*') jerry.y = round(jerry.y - 1);
    else if(scrape_char(jx, jy- 1) == '*') jerry.y = round(jerry.y + 1);

    else if(jx - 1 == tx && jy == ty) jerry.x = round(jerry.x + 2);
    else if(jx + 1 == tx && jy == ty) jerry.x = round(jerry.x - 2);
    else if(jx == tx && jy - 1 == ty) jerry.y = round(jerry.y + 2);
    else if(jx == tx && jy + 1 == ty) jerry.y = round(jerry.y - 2);


    d = sqrt(tcx*tcx + tcy* tcy);
    dx = (tcx * .25 / d);
    dy = (tcy * .25 / d);  
    if(collect_cheese)
    {
        // Movement calculation
        jerry.x = dx + jerry.x;
        jerry.y = dy + jerry.y;  
        if(round(jerry.x) == c1.x && round(jerry.y) == c1.y) c1.x = 0, c1.y = 0, game.c_active--;
        else if(round(jerry.x) == c2.x && round(jerry.y) == c2.y) c2.x = 0, c2.y = 0, game.c_active--;
        else if(round(jerry.x) == c3.x && round(jerry.y) == c3.y) c3.x = 0, c3.y = 0, game.c_active--;
        else if(round(jerry.x) == c4.x && round(jerry.y) == c4.y) c4.x = 0, c4.y = 0, game.c_active--;
        else if(round(jerry.x) == c5.x && round(jerry.y) == c5.y) c5.x = 0, c5.y = 0, game.c_active--;
    }
    if( game.currlvl > 1 && game.fw_active == 0) firework(), game.fw_active = 1;
}

void door_mechanic( void )
{
    if(game.score == 5 && game.drawn_door == 0)
    {
        int x = rand() % game.W;
        int y = rand() % game.H;
        // Check to see if out of bounds
        while(scrape_char(x, y) == '*')
        {
            x = rand() % game.W;
            y = rand() % game.H;
        }
        door.x = x;
        door.y = y - game.ob_y;
    }
}

void render_door()
{
    if(door.x != 0 && door.y != 0)
    {
        draw_char(door.x, door.y, 'X');
        game.drawn_door = 1;
    }
    if(round(jerry.x) == door.x && round(jerry.y) == door.y && !game.charswitch)
    {
        game.currlvl++;
        game.advance = 1;
        game.g_over = true;
    }
    else if(round(tom.x) == door.x && round(tom.y) == door.y && game.charswitch)
    {
        game.currlvl++;
        game.advance = 1;
        game.g_over = true;
    }
}

void mt_place( void )
{
    if(game.mt_max == 5) return;
    int x,y;
    x = rand() % game.W;
    y = rand() % game.H;
    while(scrape_char(x, y) == '*' || x > game.W || x < game.ob_x || y > game.H - 1 || y < game.ob_y)
    {
        x = rand() % game.W;
        y = rand() % game.H;
    }
    if(game.sec % game.mt_interval == 0 && game.mtpsec != game.sec)
    {
        if(game.mt_active == 5) return;
        game.mtpsec = game.sec;
        if(t1.x == 0 && t1.y == 0) t1.x = x, t1.y = y;
        else if(t2.x == 0 && t2.y == 0) t2.x = x, t2.y = y;
        else if(t3.x == 0 && t3.y == 0) t3.x = x, t3.y = y;
        else if(t4.x == 0 && t4.y == 0) t4.x = x, t4.y = y;
        else if(t5.x == 0 && t5.y == 0) t5.x = x, t5.y = y;
        game.mt_active++;
    }
}

void mt_render( void )
{
    CRENDER(t1.x, t1.y, 'M');
    CRENDER(t2.x, t2.y, 'M');
    CRENDER(t3.x, t3.y, 'M');
    CRENDER(t4.x, t4.y, 'M');
    CRENDER(t5.x, t5.y, 'M');

    // Character is controlled by jerry
    if( !game.charswitch )
    {
        if(jerry.x == t1.x && jerry.y == t1.y) t1.x = 0, t1.y = 0, game.mt_active--, game.lives--, reset();
        if(jerry.x == t2.x && jerry.y == t2.y) t2.x = 0, t2.y = 0, game.mt_active--, game.lives--, reset();
        if(jerry.x == t3.x && jerry.y == t3.y) t3.x = 0, t3.y = 0, game.mt_active--, game.lives--, reset();
        if(jerry.x == t4.x && jerry.y == t4.y) t4.x = 0, t4.y = 0, game.mt_active--, game.lives--, reset();
        if(jerry.x == t5.x && jerry.y == t5.y) t5.x = 0, t5.y = 0, game.mt_active--, game.lives--, reset();
    }
    else // Tom
    {
        if(round(jerry.x) == t1.x && round(jerry.y) == t1.y) t1.x = 0, t1.y = 0, game.mt_active--, game.score++, reset();
        if(round(jerry.x) == t2.x && round(jerry.y) == t2.y) t2.x = 0, t2.y = 0, game.mt_active--, game.score++, reset();
        if(round(jerry.x) == t3.x && round(jerry.y) == t3.y) t3.x = 0, t3.y = 0, game.mt_active--, game.score++, reset();
        if(round(jerry.x) == t4.x && round(jerry.y) == t4.y) t4.x = 0, t4.y = 0, game.mt_active--, game.score++, reset();
        if(round(jerry.x) == t5.x && round(jerry.y) == t5.y) t5.x = 0, t5.y = 0, game.mt_active--, game.score++, reset();
    }
}

void cheese_place( void )
{
    if(game.sec % game.c_interval == 0 && game.cpsec != game.sec)
    {
        if(game.c_active >= 5) return;
        game.cpsec = game.sec; 

        int x,y;
        x = rand() % game.W;
        y = rand() % game.H;
        while(scrape_char(x, y) == '*' || x > game.W || x < game.ob_x || y > game.H - 1 || y < game.ob_y)
        {
            x = rand() % game.W;
            y = rand() % game.H;
        }

        if(y <= game.ob_y) y = y + game.ob_y;
        if(c1.x == 0 && c1.y == 0) c1.x = x, c1.y = y;
        else if(c2.x == 0 && c2.y == 0) c2.x = x, c2.y = y;
        else if(c3.x == 0 && c3.y == 0) c3.x = x, c3.y = y;
        else if(c4.x == 0 && c4.y == 0) c4.x = x, c4.y = y;
        else if(c5.x == 0 && c5.y == 0) c5.x = x, c5.y = y;
        game.c_active++;
    }
}

void cheese_render()
{
    CRENDER(c1.x, c1.y, 'C');
    CRENDER(c2.x, c2.y, 'C');
    CRENDER(c3.x, c3.y, 'C');
    CRENDER(c4.x, c4.y, 'C');
    CRENDER(c5.x, c5.y, 'C');
    // Cheese interactions
    if(round(jerry.x) == c1.x && round(jerry.y) == c1.y) game.score++, c1.x = 0, c1.y = 0, game.c_active--;
    if(round(jerry.x) == c2.x && round(jerry.y) == c2.y) game.score++, c2.x = 0, c2.y = 0, game.c_active--;
    if(round(jerry.x) == c3.x && round(jerry.y) == c3.y) game.score++, c3.x = 0, c3.y = 0, game.c_active--;
    if(round(jerry.x) == c4.x && round(jerry.y) == c4.y) game.score++, c4.x = 0, c4.y = 0, game.c_active--;
    if(round(jerry.x) == c5.x && round(jerry.y) == c5.y) game.score++, c5.x = 0, c5.y = 0, game.c_active--;

}

int wall = 0;

void collision_wall( char key )
{
    int x = game.charswitch ? round(tom.x) : round(jerry.x);
    int y = game.charswitch ? round(tom.y) : round(jerry.y);
    int s = jerry.speed;
    // Using jerry.speed to directly affect movement speed
    if(!game.charswitch)
    {
        if(key == 'a' && scrape_char(round(x) - 1, y) == '*') jerry.x = x + s;
        if(key == 'd' && scrape_char(round(x) + 1, y) == '*') jerry.x = x - s;
        if(key == 'w' && scrape_char(round(x), round(y) - 1) == '*') jerry.y = y + s;
        if(key == 's' && scrape_char(round(x), round(y) + 1) == '*') jerry.y = y - s;

        jerry.x = round(jerry.x);
        jerry.y = round(jerry.y);
        tom_ai();
    }
    else
    {
        if(key == 'a' && scrape_char(round(x) - 1, y) == '*') tom.x = x + s;
        if(key == 'd' && scrape_char(round(x) + 1, y) == '*') tom.x = x - s;
        if(key == 'w' && scrape_char(round(x), round(y) - 1) == '*') tom.y = y + s;
        if(key == 's' && scrape_char(round(x), round(y) + 1) == '*') tom.y = y - s;
        tom.x = round(tom.x);
        tom.y = round(tom.y);
        jerry_ai(); // Calling TOMS ai
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

void jerry_setup( void ) 
{
    jerry.speed = 1;

    jerry.x = jerry.x * game.W;
    jerry.y = (jerry.y * game.H) + game.ob_y;

    jerry.start_x = jerry.x;
    jerry.start_y = jerry.y;

}

void tom_setup( void )
{
    tom.speed = jerry.speed / 2;

    tom.x = tom.x * game.W;
    tom.y = (tom.y * game.H) + game.ob_y;

    tom.start_x = tom.x;
    tom.start_y = tom.y;

    tom.wall = 0;
}

void tom_ob_check( void )
{
    if(tom.x < game.ob_x){tom.x++;}
    if(tom.y < game.ob_y){tom.y++;}
    if(tom.x > game.W - 1) {tom.x = game.W - 1;} 
    if(tom.y > game.H - 1) {tom.y = game.H - 1;}
}


void firework_loop()
{
    if(game.pause) return;
    double x,y;
    // bomb
    x = tom.x;
    y = tom.y;

    double t1,t2,d;

    t1 = x - fw.x;
    t2 = y - fw.y;

    d = sqrt(t1*t1 + t2*t2);

    dx = t1 * .15 / d;
    dy = t2 * .15 / d;

    fw.x = dx + fw.x;
    fw.y = dy + fw.y;
    // Check pixel helper function did not check the current pixel fast enoughah 

    if(scrape_char(round(fw.x), round(fw.y)) == '*') fw.x = 0, fw.y = 0, game.fw_active = 0;
    if(round(fw.x) == round(tom.x) && round(fw.y) == round(tom.y)) tom.x = tom.start_x, tom.y = tom.start_y, game.score++, fw.x = 0, fw.y = 0,game.fw_active = 0;
}

void controller( void )
{
    int key = get_char();
    if(key == 'r') game.lives = 0; // Instantly sends the player to the game over screen
    if(key == 'p') game.pause = (game.pause) ? false : true; 
    if(key == 'z') game.charswitch = (game.charswitch) ? 0 : 1; 
    if(key == 'l') game.advance = 1, game.g_over = true, game.currlvl++;

    collision_wall( key ); 

    if(!game.charswitch)
    {
        game.player = 'J';
        if(key == 'a' && jerry.x - 1 >= game.ob_x) jerry.x--;
        else if (key == 'd' && jerry.x + 1 < game.W) jerry.x++;
        else if (key == 'w' && jerry.y - 1 >= game.ob_y) jerry.y--;
        else if (key == 's' && jerry.y + 1 < game.H) jerry.y++;
        else if (key == 'f' && !game.fw_active && game.currlvl > 1 && !game.pause) firework(), game.fw_active = 1;

    }
    else
    {
        game.player = 'T';
        if(key == 'a' && tom.x - 1 >= game.ob_x) tom.x--;
        else if (key == 'd' && tom.x + 1 < game.W) tom.x++;
        else if (key == 'w' && tom.y - 1 >= game.ob_y) tom.y--;
        else if (key == 's' && tom.y + 1 < game.H) tom.y++;
        else if (key == 'm')
        {
            if(t1.x == 0 && t1.y == 0) t1.x = tom.x, t1.y = tom.y;
            else if(t2.x == 0 && t2.y == 0) t2.x = tom.x, t2.y = tom.y;
            else if(t3.x == 0 && t3.y == 0) t3.x = tom.x, t3.y = tom.y;
            else if(t4.x == 0 && t4.y == 0) t4.x = tom.x, t4.y = tom.y;
            else if(t5.x == 0 && t5.y == 0) t5.x = tom.x, t5.y = tom.y;
        }
        else if (key == 'c')
        {
            if(c1.x == 0 && c1.y == 0 && check_pixel(c1.x, c1.y)) c1.x = tom.x, c1.y = tom.y;
            else if(c2.x == 0 && c2.y == 0 && check_pixel(c2.x, c2.y)) c2.x = tom.x, c2.y = tom.y;
            else if(c3.x == 0 && c3.y == 0 && check_pixel(c3.x, c3.y)) c3.x = tom.x, c3.y = tom.y;
            else if(c4.x == 0 && c4.y == 0 && check_pixel(c4.x, c4.y)) c4.x = tom.x, c4.y = tom.y;
            else if(c5.x == 0 && c5.y == 0 && check_pixel(c5.x, c5.y)) c5.x = tom.x, c5.y = tom.y;
        }
    }
}

void game_logic_setup ( void )
{   
    // Game conditions
    game.g_over = false;
    game.score = 0;
    game.loop_delay = 10;

    game.pause = false;

    game.ob_x = 0;
    game.ob_y = 3;

    game.drawn_door = 0;

    game.currlvl = 1;

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
    game.min = 0;
    game.sec = 0;
    game.milsec = 0;
    game.advance = 0;

    game.charswitch = 0;

    game.exit = 0;

    // Screen
    game.W = screen_width();
    game.H = screen_height();

    reset_gameObj();
}



/*
    Initial game setup
*/
void setup ( void ) 
{
    jerry_setup();
    tom_setup();
    status_bar();
}


/*
    Timer calculation
    Conditions:
    - Game pause: false
*/
void timer( void )
{
    if ( game.pause ) return;
    if ( game.milsec == 60 ) game.sec++, game.milsec = 0;
    if ( game.sec == 60 ) game.min++, game.sec = 0;
    game.milsec++;
}


void loop( void ) 
{
    clear_screen();
    draw_int(10, 7, jerry.x);
    draw_int(10, 8, jerry.y);
    draw_int(10, 11, scrape_char(10, 10));
    game.W = screen_width();
    game.H = screen_height();

    if(fw.x != 0 && fw.y != 0) firework_loop();

    if(game.lives <= 0) game.g_over = true;
    status_bar();
    draw_wall();
    controller();
    tom_ob_check();
    timer();
    cheese_place(); // Cheese location
    mt_place(); // Mouse trap function call
    door_mechanic();
    cheese_render();
    mt_render();
    render_door();
    
    if(fw.x != 0 && fw.y != 0) draw_char(fw.x, fw.y, '!');

    draw_char(jerry.x, jerry.y, jerry.img);
    draw_char(tom.x, tom.y, tom.img);

    show_screen();
}


void game_over_screen()
{
    clear_screen();
    if(game.advance) game.firework = 1;
    !game.advance ? draw_formatted(game.W / 2 - 5, game.H / 2, "Game Over!") : draw_formatted(game.W / 2 - 8, game.H / 2, "Congratulations!");

    // Using short handed if's made this look too big
    if(!game.advance && game.currlvl - 1 < game.maxlvls){
        draw_formatted(game.W / 2 -18, game.H / 2 + 1, "Press Q to quit, or C to try again!");
    }
    else if(game.currlvl - 1 == game.maxlvls)
    {
        draw_formatted(game.W / 2 - 20, game.H / 2 + 1, "You completed the game! Press Q to quit.");
    }
    else 
    {
        draw_formatted(game.W / 2 -18, game.H / 2 + 1, "Press Q to quit, or C to continue!");
    }
    show_screen();
}


void game_loop()
{
    while ( !game.g_over ) {
        loop();
        timer_pause(game.loop_delay);
    }
}

void load_level( int level )
{
    FILE * stream = fopen(game.level[level], "r");
    if(stream != NULL)
    {
        read_file(stream);
        fclose(stream);
    }
}

int main(int argc, char *argv[]) 
{
    for (int i = 1; i < argc; i++) 
    {
        sprintf(game.level[i], "%s", argv[i]);
        game.maxlvls = i;
    }
    // Reading first level
    game.currlvl = 1;
    load_level(1);

    srand( time(NULL) );
    setup_screen();
    game_logic_setup();
    setup();

    draw_wall();
    show_screen();
    game_loop();

    while( 1 )
    {
        if(game.g_over)
        {
            int key = get_char();
            if(key == 'q') exit(1);
            else if(key == 'c' && game.currlvl - 1 != game.maxlvls)
            {
                clear_screen();
                load_level(game.currlvl);
                setup();
                draw_wall();
                show_screen();
                game_loop();
                game.g_over = 0;
                game.lives = 5;
                reset();
            }
            else
            {
                game_over_screen();
            }
            timer_pause(game.loop_delay);
        }
        else if(!game.g_over)
        {
            game_loop();
        }
        
        
    }
    return 0;
}