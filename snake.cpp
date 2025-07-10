#include <sys/ioctl.h>
#include <termios.h>  // for terminal control
#include <unistd.h>   // for read()

#include <cstdlib>  // for rand()
#include <ctime>    // for time()
#include <iostream>
#include <vector>

int WIDTH = 20;  // Default fallback if terminal size fails
int HEIGHT = 10;

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

bool useHalfSize = false;
bool gameRunning = false;
bool isPaused = false;

void clearScreen() {
    // Fast: No new process, no shell, it directly writes bytes to the terminal.
    // Portable: Works in most modern terminal emulators that support ANSI
    std::cout << "\033[2J\033[H";
}

void drawMainMenuScreen() {
    // clang-format off
    const std::vector<std::string> snakeArt = {
      "#####  ##   #   #  ##  ##  #####",
      "#       ###  #  # #  # #  ##     ",
      "#####   # ## # ##### ###   ####  ",
      "    #   #  ### #   # # #  #     ",
      "#####   #   ## #   # #  ## ##### ",
    };
    // clang-format on

    int artHeight = snakeArt.size();
    int artWidth = snakeArt[0].length();

    int offsetY = (HEIGHT / 2) - (artHeight / 2) - 2;
    int offsetX = (WIDTH / 2) - (artWidth / 2);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (y >= offsetY && y < offsetY + artHeight && x >= offsetX && x < offsetX + artWidth) {
                std::cout << snakeArt[y - offsetY][x - offsetX];
            } else {
                std::cout << EMPTY_CHAR;
            }
        }
        std::cout << std::endl;
    }

    // Print menu text after drawing art
    std::cout << "\n1. " << (gameRunning ? "Resume Game" : "Start Game") << std::endl;
    if (!gameRunning) {
        std::cout << "2. Toggle Board Size (currently: " << (useHalfSize ? "50%" : "Full") << ")\n";
    }
    std::cout << "3. Quit\n";
    std::cout << "\nEnter choice (or press M to resume): ";
}

// Generate new food position, not on top of the snake
Coord generateFoodPosition() {
    Coord newFood;
    do {
        newFood.x = std::rand() % (WIDTH - 2) + 1;
        newFood.y = std::rand() % (HEIGHT - 2) + 1;

        bool onSnake = false;
        for (const auto &part : snake) {
            if (part.x == newFood.x && part.y == newFood.y) {
                onSnake = true;
                break;
            }
        }

        if (!onSnake) break;

    } while (true);

    return newFood;
}

void initializeDimensions() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        WIDTH = w.ws_col;
        HEIGHT = w.ws_row;

        if (WIDTH < 20) WIDTH = 20;
        if (HEIGHT < 10) HEIGHT = 10;
        if (HEIGHT > 2) HEIGHT -= 1;

        if (useHalfSize) {
            WIDTH = WIDTH / 2;
            HEIGHT = HEIGHT / 2;
            // TODO: consider making verticalFrames global for dynamic snake speed
            // verticalFrames = 3;
        } else {
            // verticalFrames = 2;
        }
    } else {
        std::cerr << "Could not detect terminal size, using default." << std::endl;
    }
}

void initializeGame() {
    clearScreen();  // clear before first draw
    gameRunning = true;
    snake.clear();  // Start snake in center
    snake.push_back({WIDTH / 2, HEIGHT / 2});

    std::srand(std::time(0));  // Seed random generator

    food = generateFoodPosition();  // Place food somewhere not on the snake
}

void showMenu(bool allowResize = true) {
    clearScreen();
    drawMainMenuScreen();

    char choice;
    std::cin >> choice;

    switch (choice) {
        case '1':
        case 'm':
        case 'M':
        case 27:  // Escape key aka '\x1b'
            if (!gameRunning) {
                initializeGame();
            }
            return;  // Continue
        case '2':
            if (allowResize) {
                useHalfSize = !useHalfSize;
                initializeDimensions();
                showMenu(allowResize);
            } else {
                showMenu(allowResize);
            }
            break;
        case '3':
            exit(0);
            break;
        default:
            showMenu(allowResize);
            break;
    }
}

void drawPauseScreen() {
    // Example ASCII "PAUSE" banner
    // clang-format off
    const std::vector<std::string> pauseArt = {
        "#####   ###   #   #  ####  #####",
        "#    # #   #  #   #  #     #    ",
        "#####  #####  #   #   ###  #### ",
        "#      #   #  #   #     #  #    ",
        "#      #   #   ###   ####  #####"
    };
    // clang-format on

    int artHeight = pauseArt.size();
    int artWidth = pauseArt[0].length();

    int offsetY = (HEIGHT / 2) - (artHeight / 2);
    int offsetX = (WIDTH / 2) - (artWidth / 2);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (y >= offsetY && y < offsetY + artHeight && x >= offsetX && x < offsetX + artWidth) {
                std::cout << pauseArt[y - offsetY][x - offsetX];
            } else {
                std::cout << EMPTY_CHAR;
            }
        }
        std::cout << std::endl;
    }
}

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
            case 'p':
            case 'P':
                isPaused = !isPaused;
                break;
            case 'm':
            case 'M':
            case 27:               // Escape key aka '\x1b'
                showMenu(false);   // Do not allow resize during game
                isPaused = false;  // Auto-unpause after menu
                break;
            case 'x':
                dir = STOP;
                break;
        }
        // std::cout << "Dir: " << dir << std::endl; // uncomment line to debug key strokes
    }
}

// Returns empty string if no collision, or a reason message if collision
std::string checkCollision(const Coord &head) {
    // Check wall collision
    if (head.x == 0 || head.x == WIDTH - 1 || head.y == 0 || head.y == HEIGHT - 1) {
        return "You hit a wall!";
    }

    // Check self collision
    for (const auto &part : snake) {
        if (part.x == head.x && part.y == head.y) {
            return "You ran into yourself!";
        }
    }

    return "";
}

void updateSnake() {
    if (dir == STOP) return;  // Don't move if stopped

    // Get current head position
    Coord head = snake.back();

    // Move head in current direction
    switch (dir) {
        case UP:
            head.y--;
            break;
        case DOWN:
            head.y++;
            break;
        case LEFT:
            head.x--;
            break;
        case RIGHT:
            head.x++;
            break;
        default:
            break;
    }

    std::string reason = checkCollision(head);
    if (!reason.empty()) {
        std::cout << "Game Over! " << reason << std::endl;
        setBufferedInput(true);
        exit(0);
    }

    // Add new head to snake
    snake.push_back(head);

    // Check if head is on food
    if (head.x == food.x && head.y == food.y) {
        food = generateFoodPosition();
        // No tail removal = snake grows
    } else {
        // Remove tail to keep same length
        snake.erase(snake.begin());
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

int main() {
    // terminal throttling vertical vs horizontal due to font discrepancy
    int frameCount = 0;
    const int horizontalFrames = 1;  // Update every frame for horizontal movement
    const int verticalFrames = 2;    // Adjust this to slow down vertical if needed

    initializeDimensions();  // Get terminal size first, for proper art centering
    showMenu();
    setBufferedInput(false);  // Enable raw input

    while (true) {
        if (isPaused) {
            clearScreen();
            drawPauseScreen();
            readInput();
            usleep(100000);
            std::cout << "\033[H";  // Move cursor to home position top-left (faster than clear)
            continue;               // Skip the rest of this iteration while paused
        }

        drawBoard();
        readInput();

        // Decide which frame delay to use based on direction
        int effectiveMoveFrames = horizontalFrames;
        if (dir == UP || dir == DOWN) {
            effectiveMoveFrames = verticalFrames;
        }

        // Only move on specified frames
        if (dir != STOP && frameCount % effectiveMoveFrames == 0) {
            updateSnake();
        }

        usleep(100000);         // short delay, ~100ms
        std::cout << "\033[H";  // Move cursor to home position top-left (faster than clear)
        frameCount++;
    }

    setBufferedInput(true);  // Restore terminal settings when exiting
    return 0;
}
