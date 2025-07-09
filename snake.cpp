#include <sys/ioctl.h>
#include <termios.h>  // for terminal control
#include <unistd.h>   // for read()

#include <cstdlib>  // for rand()
#include <ctime>    // for time()
#include <iostream>
#include <vector>

const int WIDTH = 20;
const int HEIGHT = 10;

const char WALL_CHAR = '#';
const char SNAKE_HEAD_CHAR = '@';
const char SNAKE_BODY_CHAR = 'o';
const char FOOD_CHAR = '*';
const char EMPTY_CHAR = ' ';

struct Coord {
    int x;
    int y;
};

// The snake is a list of coordinates
std::vector<Coord> snake;

Coord food;

enum Direction { STOP = 0, LEFT, RIGHT, UP, DOWN };  // 0 - 4

Direction dir;

void setBufferedInput(bool enable) {
    static struct termios old;
    struct termios newt;

    if (!enable) {
        // STDIN_FILENO: file descriptor for standard input (stdin)
        tcgetattr(STDIN_FILENO, &old);  // get current terminal attributes
        newt = old;                     // copy them
        // ICANON: turns off line buffering so input is processed immediately
        // ECHO: disables echoing characters to screen when typed
        // c_lflag: local modes flag (controls various terminal behaviors)
        newt.c_lflag &= ~(ICANON | ECHO);  // disable buffering and echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);  // restore old attributes
    }
}

void readInput() {
    char c;
    // Check if there's input ready to read (non-blocking)
    int bytesWaiting;
    ioctl(0, FIONREAD, &bytesWaiting);
    if (bytesWaiting > 0) {
        read(STDIN_FILENO, &c, 1);
        switch (c) {
            case 'w':
                dir = UP;
                break;
            case 's':
                dir = DOWN;
                break;
            case 'a':
                dir = LEFT;
                break;
            case 'd':
                dir = RIGHT;
                break;
            case 'x':
                dir = STOP;
                break;
        }
        // std::cout << "Dir: " << dir << std::endl; // uncomment line to debug key strokes
    }
}

void drawBoard() {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            // Walls (top and bottom rows or first/last columns)
            if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                std::cout << WALL_CHAR;
            }
            // Food
            else if (x == food.x && y == food.y) {
                std::cout << FOOD_CHAR;
            }
            // Snake
            else {
                bool isSnakePart = false;
                for (const auto &part : snake) {
                    if (part.x == x && part.y == y) {
                        if (&part == &snake.back()) {
                            std::cout << SNAKE_HEAD_CHAR;
                        } else {
                            std::cout << SNAKE_BODY_CHAR;
                        }
                        isSnakePart = true;
                        break;
                    }
                }
                if (!isSnakePart) {
                    std::cout << EMPTY_CHAR;
                }
            }
        }
        std::cout << std::endl;
    }
}

void initializeGame() {
    // Start snake in center
    snake.clear();
    snake.push_back({WIDTH / 2, HEIGHT / 2});

    // Seed random generator
    std::srand(std::time(0));

    // Place food somewhere not on the snake
    food.x = std::rand() % (WIDTH - 2) + 1;  // avoid walls
    food.y = std::rand() % (HEIGHT - 2) + 1;
}

int main() {
    initializeGame();
    setBufferedInput(false);  // Enable raw input

    while (true) {
        drawBoard();
        readInput();

        // Simple sleep for a short delay
        usleep(200000);  // 200ms

        system("clear");
    }

    setBufferedInput(true);  // Restore terminal settings when exiting
    return 0;
}
