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
    // std clearing vs system clear
    // Faster: No new process, no new shell, it directly writes bytes to the terminal.
    // Portable: Works in most modern terminal emulators that support ANSI
    std::cout << "\033[2J\033[H";
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

void cleanupAndExit(int status) {
    setBufferedInput(true);  // Always restore echo before exit
    exit(status);
}

void drawMainMenuScreen() {
    // clang-format off
    const std::vector<std::string> snakeArt = {
      "#####   ##   #    #   ##  ##  #####",
      "#       ###  #   # #   # #    ##     ",
      "#####   # ## #  #####  ###    ####  ",
      "    #   #  ###  #   #  # #    ##     ",
      "#####   #   ##  #   #  #  #   ###### ",
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
    std::cout << "3. View Controls\n";
    std::cout << "4. Quit\n";
    if (gameRunning) {
        std::cout << "\nEnter choice (or press M or Esc to resume): ";
    } else {
        std::cout << "\nEnter choice: ";
    }
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

void showControls() {
    clearScreen();

    std::vector<std::string> lines = {"================= Controls =================",
                                      "",
                                      "During Gameplay:",
                                      "  w - Move up",
                                      "  a - Move left",
                                      "  s - Move down",
                                      "  d - Move right",
                                      "  p - Pause/Unpause",
                                      "  m or Esc - Open main menu",
                                      "  x - Stop (snake stops moving)",
                                      "",
                                      "Press any key to return to the main menu..."};

    int startY = (HEIGHT / 2) - (lines.size() / 2);
    if (startY < 0) startY = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        int lineLength = lines[i].length();
        int startX = (WIDTH / 2) - (lineLength / 2);
        if (startX < 0) startX = 0;

        std::cout << std::string(startX, ' ') << lines[i] << std::endl;
    }

    // Wait for any key press to return
    char dummy;
    clearScreen();
    read(STDIN_FILENO, &dummy, 1);
}

void showMenu(bool allowResize = true) {
    clearScreen();

    while (true) {
        drawMainMenuScreen();
        std::cout << std::flush;

        char choice;
        int bytesWaiting;
        ioctl(0, FIONREAD, &bytesWaiting);

        // Wait for valid input
        while (bytesWaiting == 0) {
            ioctl(0, FIONREAD, &bytesWaiting);
            usleep(10000);  // Sleep briefly to avoid CPU spin
        }

        read(STDIN_FILENO, &choice, 1);

        // The Escape key can be represented as:
        //   27  (decimal ASCII code)
        //   \x1b  (hexadecimal escape sequence)
        //   \033  (octal escape sequence)
        // They all mean the same single-byte Escape character.
        //
        // When arrow keys are pressed, they also start with Escape (27),
        // followed by extra bytes (e.g., '[' and 'A' for Up Arrow).
        // For example, if you press Up Arrow, your terminal typically sends:
        // [27, 91, 65]  // ESC, '[', 'A'
        // To detect a true Escape key press, we check if no extra bytes follow.
        // This avoids incorrectly interpreting arrow keys as Escape
        // and helps reduce flickering or unintended menu exits.
        if (choice == '\033') {
            // When we check if there are more bytes in the buffer (using ioctl(FIONREAD, ...)),
            // we can distinguish:
            // 1. If no extra bytes → it’s just Escape.
            // 2. If extra bytes → it’s an arrow key (or some other escape sequence).
            // This is why we read seq[0], seq[1], etc., and only treat it as Escape if nothing else
            // is there.
            ioctl(0, FIONREAD, &bytesWaiting);

            if (bytesWaiting > 0) {
                // Start building sequence buffer
                std::vector<char> seq;
                while (bytesWaiting > 0) {
                    char next;
                    read(STDIN_FILENO, &next, 1);
                    seq.push_back(next);

                    ioctl(0, FIONREAD, &bytesWaiting);
                }

                // Redraw menu and forcibly reset cursor here to reset after
                // escape keys are scraped
                if (useHalfSize) {
                    clearScreen();
                    drawMainMenuScreen();
                    std::cout << "\033[H" << std::flush;  // Move cursor to 0,0
                }

                // seq contains full sequence or multiple sequences
                // Instead of treating as plain ESC, we can safely continue and redraw
                continue;  // Ignore arrow keys, redraw menu
            }
            // If no more bytes: plain ESC (close menu)
        }

        clearScreen();  // Always clear before next decision

        if (choice == '1' || choice == 'm' || choice == 'M' || choice == '\033') {
            if (!gameRunning) {
                initializeGame();
            }
            return;  // Resume game
        } else if (choice == '2' && allowResize && !gameRunning) {
            useHalfSize = !useHalfSize;
            initializeDimensions();
        } else if (choice == '3') {
            showControls();
        } else if (choice == '4') {
            cleanupAndExit(0);
        }
        // Otherwise loop back and redraw without recursion
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

void readInput() {
    char c;
    // Check if there's input ready to read (non-blocking)
    int bytesWaiting;
    ioctl(0, FIONREAD, &bytesWaiting);
    if (bytesWaiting > 0) {
        read(STDIN_FILENO, &c, 1);

        // Ignore arrow keys: they begin with '\033' (escape sequence)
        if (c == '\033') {
            // Check if it's a full escape sequence (arrow keys)
            char seq[2];
            // Non-blocking check: if no more bytes, it's plain ESC
            ioctl(0, FIONREAD, &bytesWaiting);
            if (bytesWaiting == 0) {
                // Plain ESC pressed
                showMenu(false);
                isPaused = false;
                return;
            }
            if (read(STDIN_FILENO, &seq[0], 1) == 0) return;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) return;
            // It's an arrow key sequence, ignore it
            return;
        }

        switch (c) {
            case 'w':
                if (dir != DOWN) dir = UP;
                break;
            case 's':
                if (dir != UP) dir = DOWN;
                break;
            case 'a':
                if (dir != RIGHT) dir = LEFT;
                break;
            case 'd':
                if (dir != LEFT) dir = RIGHT;
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
            default:
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
        cleanupAndExit(0);
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

    initializeDimensions();   // Get terminal size first, for proper art centering
    setBufferedInput(false);  // Enable raw input
    showMenu();

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

    cleanupAndExit(0);
}
