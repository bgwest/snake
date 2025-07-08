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
    drawBoard();
    return 0;
}
