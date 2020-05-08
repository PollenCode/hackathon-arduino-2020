#include <LedControl.h>

#include <IRremote.h>

/* PINS */
#define MATRIX_DATA 10
#define MATRIX_CLK 11
#define MATRIX_CS 9
#define IR_RECEIVE 6
#define POTENTIOMETER A2
#define BUZZER 13

#define MATRIX_COUNT 4
#define MAZE_HEIGHT 8
#define MAZE_WIDTH 32

struct Point
{
    uint16_t x;
    uint16_t y;

    Point()
    {
    }

    Point(uint16_t x, uint16_t y) : x(x), y(y)
    {
    }
};

// https://github.com/z3t0/Arduino-IRremote/releases/tag/2.1.0
IRrecv receiver = IRrecv(IR_RECEIVE);
decode_results currentIrResult;

// https://github.com/wayoda/LedControl
LedControl controller = LedControl(MATRIX_DATA, MATRIX_CLK, MATRIX_CS, MATRIX_COUNT);

int16_t potentiometerValue = 0;

Point player = Point(0, 0);
bool maze[MAZE_WIDTH][MAZE_HEIGHT];

bool isWall(uint16_t x, uint16_t y)
{
    if (x < 0 || y < 0 || x >= MAZE_WIDTH || y >= MAZE_HEIGHT)
        return false;
    return maze[x][y];
}

void generateMaze()
{
    Point current = Point(MAZE_WIDTH / 2, MAZE_HEIGHT / 2); // start in the middle

    uint16_t returningPointsSize = 0;
    Point returning[((MAZE_WIDTH - 1) / 2) * ((MAZE_HEIGHT - 1) / 2)];

    for (uint16_t x = 0; x < MAZE_WIDTH; x++)
        for (uint16_t y = 0; y < MAZE_HEIGHT; y++)
            maze[x][y] = true; // set everything to wall first

    while (true)
    {
        bool directions[4];
        directions[0] = isWall(current.x, current.y - 2); // up
        directions[1] = isWall(current.x + 2, current.y); // right
        directions[2] = isWall(current.x, current.y + 2); // down
        directions[3] = isWall(current.x - 2, current.y); // left

        uint8_t ableDirections = 0;
        for (uint8_t i = 0; i < 4; i++)
            if (directions[i])
                ableDirections++;

        if (ableDirections == 0)
        {
            if (returningPointsSize == 0)
                break; // maze generation done

            current = returning[--returningPointsSize];
        }
        else
        {
            if (ableDirections > 1)
            {
                // save point
                returning[returningPointsSize++] = current;
            }

            uint8_t directionIndex = random(ableDirections);
            uint8_t direction = 0;
            for (uint8_t i = 0; i < 4; i++)
            {
                if (directions[i] && directionIndex-- == 0)
                {
                    direction = i;
                    break;
                }
            }

            // go to random direction
            if (direction == 0)
            {
                maze[current.x][current.y - 1] = false; // create path
                maze[current.x][current.y - 2] = false;
                current.y -= 2;
            }
            else if (direction == 1)
            {
                maze[current.x + 1][current.y] = false; // create path
                maze[current.x + 2][current.y] = false;
                current.x += 2;
            }
            else if (direction == 2)
            {
                maze[current.x][current.y + 1] = false; // create path
                maze[current.x][current.y + 2] = false;
                current.y += 2;
            }
            else // if (direction == 3)
            {
                maze[current.x - 1][current.y] = false; // create path
                maze[current.x - 2][current.y] = false;
                current.x -= 2;
            }
        }
    }
}

void setup()
{
    pinMode(POTENTIOMETER, INPUT);
    pinMode(BUZZER, OUTPUT);
    randomSeed(analogRead(A0) + micros()); // initialize the random state machine

    Serial.begin(9600);

    clearDisplay();
    receiver.enableIRIn();

    generateMaze();
    printMaze();
    updateDisplay();
}

void clearDisplay()
{
    for (uint16_t i = 0; i < MATRIX_COUNT; i++)
    {
        controller.shutdown(i, false);
        controller.setIntensity(i, 8);
        controller.clearDisplay(i);
    }
}

void updateDisplay()
{
    for (int x = 0; x < MATRIX_COUNT * 8; x++)
    {
        int device = MATRIX_COUNT - 1 - x / 8;
        int column = x % 8;

        if (x >= MAZE_WIDTH)
        {
            Serial.println("Warning: updateDisplay() overflowed.");
            break;
        }

        uint8_t value = 0x0;
        for (int y = 0; y < 8; y++)
            value |= maze[x][y] << y;

        Serial.print(value, BIN);
        Serial.print(" at device ");
        Serial.print(device);
        Serial.print(", column ");
        Serial.println(column);

        controller.setColumn(device, column, value);
    }
}

void printMaze()
{
    for (int y = 0; y < MAZE_HEIGHT; y++)
    {
        for (int x = 0; x < MAZE_WIDTH; x++)
            Serial.print(maze[x][y] ? '%' : ' ');
        Serial.println();
    }
}

void loop()
{
    if (receiver.decode(&currentIrResult))
    {
        Serial.print("Received ir signal: ");
        Serial.println(currentIrResult.value);

        receiver.resume();
    }

    int16_t pot = analogRead(POTENTIOMETER);
    if (abs(potentiometerValue - pot) > 100)
    {
        potentiometerValue = pot;
        tone(BUZZER, potentiometerValue * 2 + 400, 100);
    }
}
