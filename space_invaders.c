#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#define F_CPU 8000000UL
#define BAUD 38400
#define UBRR_VALUE ((F_CPU/(8UL * BAUD)) - 1UL)

#define ROWS 20
#define COLUMNS 40
#define MAX_ENEMIES 12  

#define ENEMY ((unsigned char)'W')
#define PLAYER ((unsigned char)'^')
#define BULLET ((unsigned char)'*')
#define ENEMY_BULLET_LIMIT (MAX_ENEMIES / 2)

uint8_t enemy_move_count;
uint8_t enemy_direction_changed;
unsigned char enemy_move_direction = 'r'; //l = left; r = right
unsigned char player_move_buffer; 
uint8_t player_bullet_active = 0;
uint8_t enemy_death_counter;
uint8_t player_collision = 0;
uint8_t check_player_collision = 0;

volatile uint8_t player_ticks;
volatile uint8_t player_bullet_ticks;
volatile uint8_t enemy_ticks;
volatile uint8_t enemy_bullet_ticks;

uint8_t rendering = 0;

typedef struct {
    uint8_t row;
    uint8_t column;
} Position;

typedef struct {
    Position pos;
    uint8_t alive;
} Enemy;

typedef struct {
    Position pos;
    uint8_t active;
} EnemyBullet;

Position player = {ROWS - 1, COLUMNS / 2};
Position cursor = {0, 0};
Position player_bullet;
Enemy enemies[MAX_ENEMIES];
EnemyBullet enemy_bullets[ENEMY_BULLET_LIMIT];

void uart_init(void) {
    UBRR0H = (unsigned char)(UBRR_VALUE >> 8);
    UBRR0L = (unsigned char)(UBRR_VALUE & 0xFF);

    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void timer_init(void) {
    TCNT0 = 0;
    TCCR0A = 0x00;
    TIMSK0 = (1 << TOIE0);
    TCCR0B = (1 << CS02); 
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

int uart_putchar(char c, FILE *stream) {
    uart_transmit(c);
    return 0;
}

void uart_transmit_string(const char *str) {
    while (*str) {
        uart_transmit(*str);
        str++;
    }
}

void generate_enemies(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES / 2; i++) {
        enemies[i].pos.row = 0;
        enemies[i].pos.column = i;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES / 2; i++) {
        enemies[i + (MAX_ENEMIES / 2)].pos.row = 1;
        enemies[i + (MAX_ENEMIES / 2)].pos.column = i;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].alive = 1;
    }
    
    
}

void initialize_enemy_bullet_array(void) {
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        enemy_bullets[i].pos.row = 0;
        enemy_bullets[i].pos.column = 0;
        enemy_bullets[i].active = 0;
    }
}

void overwrite_position(unsigned char new_char, Position position) {
    int row_difference = position.row - cursor.row;
    int column_difference = position.column - cursor.column;

    //Checks if the cursor should move forward or backward/left or right
    if (row_difference < 0) {
        printf("\x1b[%dA", -row_difference);
    } else if (row_difference > 0) {
        printf("\x1b[%dB", row_difference);
    }

    if (column_difference < 0) {
        printf("\x1b[%dD", -column_difference);
    } else if(column_difference > 0) {
        printf("\x1b[%dC", column_difference);
    }

    //Writes a new character and moves back to the argument position
    printf("%c\x1b[1D", new_char);
    //Updates cursor position
    cursor = position;
}

void move_enemies_right(void) {
    enemy_move_count++;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        Position new = enemies[i].pos;
        new.column++;

        if (enemies[i].alive == 1) {
            overwrite_position(ENEMY, new);
        } else {
            overwrite_position(' ', new);
        }
    }

    overwrite_position(' ', enemies[0].pos);
    overwrite_position(' ', enemies[MAX_ENEMIES / 2].pos);
    
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].pos.column++;
    }
    
}

void move_enemies_left(void) {
    enemy_move_count++;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        Position new = enemies[i].pos;
        new.column--;

        if (enemies[i].alive == 1) {
            overwrite_position(ENEMY, new);
        } else {
            overwrite_position(' ', new);
        }
    }
    
    overwrite_position(' ', enemies[MAX_ENEMIES / 2 - 1].pos);
    overwrite_position(' ', enemies[MAX_ENEMIES - 1].pos);

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].pos.column--;
    }
    
}

void move_enemies_down(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        Position new_pos = enemies[i].pos;
        new_pos.row++;

        if (enemies[i].alive == 1) {
            overwrite_position(ENEMY, new_pos);
        } else {
            overwrite_position(' ', new_pos);
        }
    }

    for (uint8_t i = 0; i < MAX_ENEMIES / 2; i++) {
        overwrite_position(' ', enemies[i].pos);
    }
    

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].pos.row++;
    }

    if (enemies[MAX_ENEMIES - 1].pos.row == (ROWS - 1)) {
        check_player_collision = 1;
    }
}

void shoot_enemy_bullet(void) {
    if (rendering == 1) return;
    rendering = 1;

    int first_empty_index = -1;
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        if (enemy_bullets[i].active == 0) {
            first_empty_index = i;
            break;
        } 
    }
    
    if (first_empty_index < 0) {
        rendering = 0;
        return; //returns if all bullets already on screen
    }

    for (int i = MAX_ENEMIES - 1; i >= 0; i--) {
        if (enemies[i].alive == 1) {
            enemy_bullets[first_empty_index].pos = enemies[i].pos;
            enemy_bullets[first_empty_index].pos.row++;

            enemy_bullets[first_empty_index].active = 1;
            overwrite_position(BULLET, enemy_bullets[first_empty_index].pos);

            rendering = 0;
            return;
        }
    }

    rendering = 0;
}

void move_enemy_bullets(void) {
    if (rendering == 1) return;
    rendering = 1;

    //Check if bullets collide with player bullet
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        if (enemy_bullets[i].pos.row == player_bullet.row &&
            enemy_bullets[i].pos.column == player_bullet.column &&
            enemy_bullets[i].active == 1 &&
            player_bullet_active == 1)
        {
            overwrite_position(' ', player_bullet);
            enemy_bullets[i].active = 0;
            player_bullet_active = 0;
        }
    }
    
    //Check if bullets collide with player
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        if (enemy_bullets[i].active && 
            enemy_bullets[i].pos.row == player.row &&
            enemy_bullets[i].pos.column == player.column)
        {
            player_collision = 1;
            rendering = 0;
            return;
        }
        
    }

    //Check if bullets will be out of bounds
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        if (enemy_bullets[i].pos.row >= (ROWS - 1) &&
            enemy_bullets[i].active == 1)
        {
            enemy_bullets[i].active = 0;
            overwrite_position(' ', enemy_bullets[i].pos);
        }
    }
    
    //Move bullets
    for (uint8_t i = 0; i < ENEMY_BULLET_LIMIT; i++) {
        if (enemy_bullets[i].active == 1) {
            overwrite_position(' ', enemy_bullets[i].pos);
            enemy_bullets[i].pos.row++;
            overwrite_position(BULLET, enemy_bullets[i].pos);
        }
    }

    rendering = 0;
}

void move_enemies(void) {
    if (rendering == 1) {
        return;
    }
    rendering = 1;

    if (enemy_direction_changed >= 3) {
        enemy_direction_changed = 0;
        move_enemies_down();
    } else if (enemy_move_direction == 'l') {
        move_enemies_left();
    } else if (enemy_move_direction == 'r') {
        move_enemies_right();
    }

    //Logic to check in which direction enemies should move
    if (enemy_move_count >= (COLUMNS - MAX_ENEMIES / 2)) {
        enemy_move_count = 0;
        enemy_move_direction = (enemy_move_direction == 'l') ? 'r' : 'l';
        enemy_direction_changed++;
    } 
    
    rendering = 0;
}

void move_player_left(void) {
    if (rendering == 1) {
        return;
    }
    rendering = 1;

    Position new_player_pos = player;
    new_player_pos.column--;
    overwrite_position(PLAYER, new_player_pos);
    overwrite_position(' ', player);

    cursor = player;
    player = new_player_pos;
    player_move_buffer = ' ';

    rendering = 0;
}

void move_player_right(void) {
    if (rendering == 1) {
        return;
    }
    rendering = 1;

    
    Position new_player_pos = player;
    new_player_pos.column++;
    overwrite_position(PLAYER, new_player_pos);
    overwrite_position(' ', player);

    cursor = player;
    player = new_player_pos;
    player_move_buffer = ' ';

    rendering = 0;
}

void shoot_player_bullet(void) {
    if (rendering == 1) {
        return;
    }
    rendering = 1;

    player_bullet = player;
    player_bullet.row--;

    overwrite_position(BULLET, player_bullet);
    player_bullet_active = 1;
    player_move_buffer = ' ';

    rendering = 0;
}

void move_player_bullet(void) {
    if (rendering == 1) {
        return;
    }
    rendering = 1;

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].pos.row == player_bullet.row &&
            enemies[i].pos.column == player_bullet.column &&
            enemies[i].alive == 1) 
        {   
            enemies[i].alive = 0;
            player_bullet_active = 0;
            enemy_death_counter++;

            rendering = 0;
            return;
        }
    }

    if (player_bullet.row == 0) {
        overwrite_position(' ', player_bullet);
        player_bullet_active = 0;

        rendering = 0;
        return;
    }

    overwrite_position(' ', player_bullet);
    player_bullet.row--;
    overwrite_position(BULLET, player_bullet);

    rendering = 0;
}

void process_input(void) {
    if (player_move_buffer == 'a' && player.column != 0) {
        move_player_left();
    }

    if (player_move_buffer == 'd' && player.column != (COLUMNS - 1)) {
        move_player_right();
    }

    if (player_move_buffer == 'w' && player_bullet_active == 0) {
        shoot_player_bullet();
    }
}

void check_player_collison(void) {
    for (uint8_t i = MAX_ENEMIES / 2; i < MAX_ENEMIES; i++) {
        if (player.row == enemies[i].pos.row && player.column == enemies[i].pos.column) {
            player_collision = 1;
        }
    }
}

void render_game_field(void) {
    for (uint8_t i = 0; i < COLUMNS + 2; i++) {
        uart_transmit('#');
    }
    uart_transmit_string("\r\n");

    for (uint8_t i = 0; i < ROWS; i++) {
        uart_transmit('#');
        for (uint8_t j = 0; j < COLUMNS; j++) {
            if (i <= 1 && j <= (MAX_ENEMIES / 2 - 1)) {
                uart_transmit(ENEMY);
            } else if (i == player.row && j == player.column) {
                uart_transmit(PLAYER);
            } else {
                uart_transmit(' ');
            }
        }
        
        uart_transmit_string("#\r\n");
    }

    for (uint8_t i = 0; i < COLUMNS + 2; i++) {
        uart_transmit('#');
    }
    
    
    //Set correct cursor position {0, 0}
    printf("\x1b[%dA", ROWS);
    printf("\x1b[%dD", (COLUMNS + 1));
}

FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main(void) {
    uart_init();
    stdout = &uart_output;
    printf("\x1b[?25l");
    printf("\x1b[1D");
    printf(" "); //Hides cursor and deletes garbage character (bug)
    printf("\x1b[1D");
    generate_enemies();
    initialize_enemy_bullet_array();
    render_game_field();

    timer_init();
    sei();

    while(1) {
        if (enemy_death_counter == MAX_ENEMIES) {
            cli();
            printf("\x1b[2J");
            printf("\x1b[H");
            printf("You won!!");
            break; 
        }

        if (player_collision == 1) {
            cli();
            printf("\x1b[2J");
            printf("\x1b[H");
            printf("You lost!!");
            break;
        }
        if (player_ticks >= 2) {
            player_ticks = 0;
            process_input();
            check_player_collison();
        }

        if (player_bullet_ticks >= 2 && player_bullet_active == 1) {
            player_bullet_ticks = 0;
            move_player_bullet();
        }
        
        if (enemy_ticks % 6 == 0) {
            move_enemies();
        }

        if (enemy_ticks >= 12) {
            enemy_ticks = 0;
            move_enemy_bullets();
        }
        
        if (enemy_bullet_ticks >= 60) {
            enemy_bullet_ticks = 0;
            shoot_enemy_bullet();
        }
    }
}

ISR(TIMER0_OVF_vect) {
    player_ticks++;
    enemy_ticks++;
    player_bullet_ticks++;
    enemy_bullet_ticks++;
}

ISR(USART_RX_vect) {
    player_move_buffer = UDR0;
}