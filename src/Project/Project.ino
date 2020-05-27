#include <LedControl.h>
#include <IRremote.h>

/* PINS */
#define MATRIX_DATA 10
#define MATRIX_CLK 11
#define MATRIX_CS 9
#define IR_RECEIVE 6
#define POTENTIOMETER A2
#define BUZZER 13
#define FLAME A3
#define TILT 7

#define MATRIX_COUNT 4
#define FIELD_WIDTH 32
#define FIELD_HEIGHT 8

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

typedef uint16_t coord;

struct Point
{
    coord x, y;

    Point() {}
    Point(coord x, coord y) : x(x), y(y) {}
};

struct Rect
{
    coord x, y, w, h;

    Rect() {}
    Rect(coord x, coord y, coord w, coord h) : x(x), y(y), w(w), h(h) {}
};

// https://github.com/z3t0/Arduino-IRremote/releases/tag/2.1.0
IRrecv receiver = IRrecv(IR_RECEIVE);
decode_results currentIrResult;

// https://github.com/wayoda/LedControl
LedControl controller = LedControl(MATRIX_DATA, MATRIX_CLK, MATRIX_CS, MATRIX_COUNT);

bool left, right, notFirstTime = false;

Point players[4];
Point playerGoals[4]; // = Point(FIELD_WIDTH - 2, 6);
bool maze[FIELD_WIDTH][FIELD_HEIGHT];

uint16_t tick;
uint64_t lastTiltMillis = 0;

bool isPath(coord x, coord y)
{
    if (x < 0 || y < 0 || x >= FIELD_WIDTH || y >= FIELD_HEIGHT)
        return false;
    return !maze[x][y];
}

bool isWall(coord x, coord y, const Rect rect)
{
    if (x < rect.x || y < rect.y || x >= rect.x + rect.w || y >= rect.y + rect.h)
        return false;
    return maze[x][y];
}

void generateMaze(const Rect rect)
{
    Point current = Point(rect.x + rect.w / 2, rect.y + rect.h / 2); // start in the middle

    uint16_t returningPointsSize = 0;
    Point returning[((FIELD_WIDTH - 1) / 2) * ((FIELD_HEIGHT - 1) / 2)];

    for (coord xi = rect.x; xi < rect.x + rect.w; xi++)
        for (coord yi = rect.y; yi < rect.y + rect.h; yi++)
            maze[xi][yi] = true; // set everything to wall first

    bool directions[4];
    while (true)
    {
        directions[0] = isWall(current.x, current.y - 2, rect); // up
        directions[1] = isWall(current.x + 2, current.y, rect); // right
        directions[2] = isWall(current.x, current.y + 2, rect); // down
        directions[3] = isWall(current.x - 2, current.y, rect); // left

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

void moveAllPlayers(uint8_t dir)
{
    Serial.print("Moving all players in dir ");
    Serial.println(dir);

    tone(BUZZER, 400 + dir * 200, 50);

    for (uint8_t i = 0; i < 4; i++)
    {
        Point &p = players[i];
        const Point &g = playerGoals[i];
        movePlayer(p, dir);

        if (p.x == g.x && p.y == g.y)
        {
            // show i-th number
        }
        else
        {
            // hide i-th number
        }
    }
}

void movePlayer(Point &player, uint8_t dir)
{
    switch (dir)
    {
    case UP:
        if (isPath(player.x, player.y - 1))
        {
            do
            {
                setLed(player, false);
                player.y -= 1;
                setLed(player, true);
            } while (isPath(player.x, player.y - 1) && !isPath(player.x - 1, player.y) && !isPath(player.x + 1, player.y));
        }
        break;
    case RIGHT:
        if (isPath(player.x + 1, player.y))
        {
            do
            {
                setLed(player, false);
                player.x += 1;
                setLed(player, true);
            } while (isPath(player.x + 1, player.y) && !isPath(player.x, player.y - 1) && !isPath(player.x, player.y + 1));
        }
        break;
    case DOWN:
        if (isPath(player.x, player.y + 1))
        {
            do
            {
                setLed(player, false);
                player.y += 1;
                setLed(player, true);
            } while (isPath(player.x, player.y + 1) && !isPath(player.x - 1, player.y) && !isPath(player.x + 1, player.y));
        }
        break;
    case LEFT:
        if (isPath(player.x - 1, player.y))
        {
            do
            {
                setLed(player, false);
                player.x -= 1;
                setLed(player, true);
            } while (isPath(player.x - 1, player.y) && !isPath(player.x, player.y - 1) && !isPath(player.x, player.y + 1));
        }
        break;
    }
}

void generateMazes()
{
    for (uint8_t i = 0; i < MATRIX_COUNT; i++)
    {
        generateMaze(Rect(i * 8, 0, 8, 8));
    }
}

void setup()
{
    pinMode(FLAME, INPUT);
    pinMode(TILT, INPUT_PULLUP);
    pinMode(POTENTIOMETER, INPUT);
    pinMode(BUZZER, OUTPUT);
    randomSeed(analogRead(A0) + micros()); // initialize the random state machine

    Serial.begin(9600);

    clearDisplay();
    receiver.enableIRIn();

    players[0] = Point(6, 0);
    players[1] = Point(8 + 0, 0);
    players[2] = Point(16 + 6, 6);
    players[3] = Point(24 + 0, 6);

    playerGoals[0] = Point(0, 6);
    playerGoals[1] = Point(8 + 6, 6);
    playerGoals[2] = Point(16 + 0, 0);
    playerGoals[3] = Point(24 + 6, 0);

    generateMazes();
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
        int device = x / 8;
        int column = x % 8;

        if (x >= FIELD_WIDTH)
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

        //controller.setRow(device, column, B10101010);
        controller.setColumn(device, 7 - column, value);
    }
}

void printMaze()
{
    for (int y = 0; y < FIELD_HEIGHT; y++)
    {
        for (int x = 0; x < FIELD_WIDTH; x++)
            Serial.print(maze[x][y] ? '%' : ' ');
        Serial.println();
    }
}

void setLed(Point point, bool on)
{
    controller.setLed(point.x / 8, 7 - point.y, 7 - point.x % 8, on);
}

bool fireState = false;

void loop()
{
    if (receiver.decode(&currentIrResult))
    {
        //Serial.print("Received ir signal: ");
        //Serial.println(currentIrResult.value);

        switch (currentIrResult.value)
        {
        case 16718055:
            moveAllPlayers(UP);
            break;
        case 16734885:
            moveAllPlayers(RIGHT);
            break;
        case 16730805:
            moveAllPlayers(DOWN);
            break;
        case 16716015:
            moveAllPlayers(LEFT);
            break;
        }

        receiver.resume();
    }

    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 'w')
            moveAllPlayers(UP);
        else if (c == 'a')
            moveAllPlayers(LEFT);
        else if (c == 's')
            moveAllPlayers(DOWN);
        else if (c == 'd')
            moveAllPlayers(RIGHT);
    }

    int16_t pot = analogRead(POTENTIOMETER);
    if (pot < 200 && !right)
    {
        right = true;
        left = false;
        if (notFirstTime)
            moveAllPlayers(DOWN);
        notFirstTime = true;
    }
    else if (pot > 1024 - 200 && !left)
    {
        right = false;
        left = true;
        if (notFirstTime)
            moveAllPlayers(DOWN);
        notFirstTime = true;
    }

    int16_t fire = analogRead(FLAME);
    if (fire > 300 && !fireState)
    {
        fireState = true;
        moveAllPlayers(RIGHT);
    }
    else if (fire < 75 && fireState)
    {
        fireState = false;
    }

    if (digitalRead(TILT) && millis() - lastTiltMillis > 800)
    {
        lastTiltMillis = millis();
        moveAllPlayers(UP);
    }

    for (uint8_t i = 0; i < 4; i++)
        setLed(players[i], tick % 30 < 15);
    for (uint8_t i = 0; i < 4; i++)
        setLed(playerGoals[i], tick % 2);

    tick++;
}
