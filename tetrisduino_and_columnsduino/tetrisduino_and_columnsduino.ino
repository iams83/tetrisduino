
#include <TFT.h>
#include <SPI.h>
#include <Esplora.h>
#include <EEPROM.h>

class Color
{
public:
    byte r, g, b;

    Color() : r(0), g(0), b(0)
    {
    }

    Color(long rgb) :
        r((rgb >> 16) & 255),
        g((rgb >> 8) & 255),
        b((rgb >> 0) & 255)
    {
    }

    Color(byte r, byte g, byte b) : r(r), g(g), b(b)
    {
    }

    static Color fromHSV(byte h, byte s, byte v)
    {
        Color rgb;
        unsigned char region, remainder, p, q, t;

        if (s == 0)
        {
            rgb.r = v;
            rgb.g = v;
            rgb.b = v;
            return rgb;
        }

        region = h / 43;
        remainder = (h - (region * 43)) * 6;

        p = (v * (255 - s)) >> 8;
        q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

        switch (region)
        {
            case 0:
                rgb.r = v; rgb.g = t; rgb.b = p;
                break;
            case 1:
                rgb.r = q; rgb.g = v; rgb.b = p;
                break;
            case 2:
                rgb.r = p; rgb.g = v; rgb.b = t;
                break;
            case 3:
                rgb.r = p; rgb.g = q; rgb.b = v;
                break;
            case 4:
                rgb.r = t; rgb.g = p; rgb.b = v;
                break;
            default:
                rgb.r = v; rgb.g = p; rgb.b = q;
                break;
        }

        return rgb;
    }

    void EsploraTFT_background() const
    {
        EsploraTFT.background(this->b, this->g, this->r);
    }

    void EsploraTFT_stroke() const
    {
        EsploraTFT.stroke(this->b, this->g, this->r);
    }

    void EsploraTFT_fill() const
    {
        EsploraTFT.fill(this->b, this->g, this->r);
    }
};

namespace BasicColors
{
    Color WHITE  (0xFFFFFFL);
    Color SILVER (0xC0C0C0L);
    Color GRAY   (0x808080L);
    Color BLACK  (0x000000L);
    Color RED    (0xFF0000L);
    Color MAROON (0x800000L);
    Color YELLOW (0xFFFF00L);
    Color OLIVE  (0x808000L);
    Color LIME   (0x00FF00L);
    Color GREEN  (0x008000L);
    Color AQUA   (0x00FFFFL);
    Color TEAL   (0x008080L);
    Color BLUE   (0x0000FFL);
    Color NAVY   (0x000080L);
    Color FUCHSIA(0xFF00FFL);
    Color PURPLE (0x800080L);

    Color LIGHT_GRAY(0xC0C0C0L);
    Color DARK_GRAY(0x404040L);
}

class Retarder
{
    long time;

    long nextTimeout;

public:
    Retarder(long time) : time(time)
    {
        this->restart();
    }

    void setTime(long time)
    {
        this->time = time;
        this->restart();
    }

    bool expired()
    {
        if (millis() > this->nextTimeout)
        {
            this->restart();

            return true;
        }

        return false;
    }

    void restart(int addDelay = 0)
    {
        this->nextTimeout = millis() + this->time + addDelay;
    }
};

class _SwitchesManager
{
    bool clearedKey[20];

public:
    bool isPressed(int button)
    {
        bool switchToggled = Esplora.readButton(button) == LOW;

        if (this->clearedKey[button] && switchToggled)
        {
            this->clearedKey[button] = false;
            return true;
        }
        else if (!switchToggled)
        {
            this->clearedKey[button] = true;
        }

        return false;
    }
};

_SwitchesManager SwitchesManager;

const char *SOUND_ICON =
        "    #  # "
        "   ##   #"
        "##### # #"
        "##### # #"
        "##### # #"
        "   ##   #"
        "    #  # ";

const char *NO_SOUND_ICON =
        "    #    "
        "   ##    "
        "##### # #"
        "#####  # "
        "##### # #"
        "   ##    "
        "    #    ";

class _SoundManager
{
    bool isFXEnabled;

public:
    Color background;

    long showIconTime;

    _SoundManager() : isFXEnabled(true), showIconTime(0)
    {
    }

    void playFXSound(int freq, int duration)
    {
        if (isFXEnabled)
        {
            Esplora.noTone();
            Esplora.tone(freq, duration);
        }
    }

    void loop()
    {
        int currentMillis = millis();

        if (SwitchesManager.isPressed(SWITCH_UP))
        {
            Esplora.noTone();
            this->isFXEnabled = !this->isFXEnabled;
            this->showIconTime = currentMillis + 1000;
        }

        int offx = EsploraTFT.width() - 11;

        if (this->showIconTime > 0)
        {
            if (currentMillis < this->showIconTime)
            {
                char const *icon = this->isFXEnabled ? SOUND_ICON : NO_SOUND_ICON;

                for (int i = 0; i < 7; i ++)
                for (int j = 0; j < 9; j ++)
                {
                    if (*icon == ' ')
                        this->background.EsploraTFT_stroke();
                    else
                        BasicColors::YELLOW.EsploraTFT_stroke();

                    EsploraTFT.point(offx + j, 2 + i);

                    icon ++;
                }
            }
            else
            {
                this->background.EsploraTFT_fill();
                EsploraTFT.rect(offx, 2, 9, 7);
                this->showIconTime = 0;
            }
        }
    }
};

_SoundManager SoundManager;

class _PermanentStorage
{
    enum { EEPROM_SIZE = 1024 };
    enum { BLOCK_SIZE = 16 };

    enum { NUM_BLOCKS = EEPROM_SIZE / BLOCK_SIZE };

    int findEmptyBlock()
    {
        for (int blockNumber = 0; blockNumber < NUM_BLOCKS; blockNumber ++)
        {
            if (EEPROM.read(blockNumber * BLOCK_SIZE) == 0)
                return blockNumber;
        }

        return -1;
    }

    int findBlock(const char *requestedLabel)
    {
        char currentLabel[BLOCK_SIZE + 1];

        for (int blockNumber = 0; blockNumber < NUM_BLOCKS; blockNumber ++)
        {
            for (int i = 0; i < BLOCK_SIZE; i ++)
            {
                int v = EEPROM.read(blockNumber * BLOCK_SIZE + i);

                currentLabel[i] = v;

                if (!v)
                    break;
            }

            currentLabel[BLOCK_SIZE] = 0;

            if (strcmp(currentLabel, requestedLabel) == 0)
            {
                return blockNumber;
            }
        }

        return -1;
    }

    void readBlockValue(int blockNumber, char *value)
    {
        int i = 0;

        while (i < BLOCK_SIZE)
        {
            int v = EEPROM.read(blockNumber * BLOCK_SIZE + i);
            i ++;

            if (!v)
                break;
        }

        while (i < BLOCK_SIZE)
        {
            *value = EEPROM.read(blockNumber * BLOCK_SIZE + i);

            i ++;

            value ++;
        }
    }

    void writeBlock(int blockNumber, const char *label, const char *value)
    {
        int i = 0;

        while (i < BLOCK_SIZE && *label)
        {
            EEPROM.write(blockNumber * BLOCK_SIZE + i, *label);
            i ++;
            label ++;
        }

        EEPROM.write(blockNumber * BLOCK_SIZE + i, 0);
        i ++;

        while (i < BLOCK_SIZE)
        {
            EEPROM.write(blockNumber * BLOCK_SIZE + i, *value);
            i ++;
            value ++;
        }
    }

public:
    void format()
    {
        for (int pos = 0; pos < EEPROM_SIZE; pos += BLOCK_SIZE)
            EEPROM.write(pos, 0);
    }

    bool writeBlockInt(const char *label, int value)
    {
        int pos = findBlock(label);

        if (pos == -1)
            pos = findEmptyBlock();

        if (pos == -1)
            return false;

        char valueS[BLOCK_SIZE];
        valueS[0] = value >> 24;
        valueS[1] = value >> 16;
        valueS[2] = value >> 8;
        valueS[3] = value >> 0;

        writeBlock(pos, label, valueS);
        return true;
    }

    bool readBlockInt(const char *label, int &value)
    {
        int pos = findBlock(label);

        if (pos == -1)
            return false;

        char valueS[BLOCK_SIZE];

        readBlockValue(pos, valueS);

        value = valueS[0] & 0xFF; value <<= 8;
        value |= valueS[1] & 0xFF; value <<= 8;
        value |= valueS[2] & 0xFF; value <<= 8;
        value |= valueS[3] & 0xFF;

        return true;
    }

};

_PermanentStorage PermanentStorage;

namespace ActivityManager
{
    enum
    {
        GAME_START,
        ONGOING_GAME,
        GAME_OVER

    } Activity;

    class ActivityLoop
    {
    public:
        virtual void initialize() = 0;

        virtual void loop() = 0;
    };

    void SetGameActivity(int activity);

    void StartTetris();

    void StartColumns();
}

namespace BoardGame
{
    enum { BOARD_OFFSET_X = 18 };
    enum { BOARD_OFFSET_Y = 2 };

    enum { NEXT_PIECE_OFFSET_X = 110 };
    enum { NEXT_PIECE_OFFSET_Y = 20 };

    enum { DISPLAY_WIDTH  = 160 };
    enum { DISPLAY_HEIGHT = 128 };

    class Piece
    {
    public:
        virtual void rotate(bool dir) = 0;
    };

    class TetrisPiece : public Piece
    {
        bool p[5][5];

    public:
        TetrisPiece(const char *p0 = 0)
        {
            if (p0)
            {
                for (int x = 0; x < 5; x ++)
                for (int y = 0; y < 5; y ++)
                {
                    this->p[x][y] = *p0 == ' ' ? false : true;

                    p0 ++;
                }
            }
        }

        void rotate(bool dir)
        {
            TetrisPiece buffer(*this);

            for (int j = 0; j < 5; j ++)
            for (int i = 0; i < 5; i ++)
                this->p[i][j] = dir ? buffer.p[j][4 - i] : buffer.p[4 - j][i];
        }

        bool get(int x, int y) const
        {
            return this->p[x][y];
        }
    };

    class ColumnsPiece : public Piece
    {
        byte p[3];

    public:
        ColumnsPiece()
        {
            this->p[0] = random(7);
            this->p[1] = random(7);
            this->p[2] = random(7);
        }

        void rotate(bool dir)
        {
            if (dir)
            {
                byte t = this->p[0];
                this->p[0] = this->p[1];
                this->p[1] = this->p[2];
                this->p[2] = t;
            }
            else
            {
                byte t = this->p[2];
                this->p[2] = this->p[1];
                this->p[1] = this->p[0];
                this->p[0] = t;
            }
        }

        byte get(byte n) const
        {
            return this->p[n];
        }
    };

   const Color ColorsList[] =
    {
        Color(0xFF4444L),
        Color(0x00FF00L),
        Color(0x6666FFL),
        Color(0xAAAAAAL),
        Color(0x00FFDDL),
        Color(0xFF00FFL),
        Color(0xFFFF00L)
    };

    class Board
    {
        enum { MAX_BOARD_WIDTH  =  9 };
        enum { MAX_BOARD_HEIGHT = 21 };

        enum { CLEARED_CELL = 255 };

        byte p[MAX_BOARD_HEIGHT][MAX_BOARD_WIDTH];

        byte numCols, numRows, pieceSize;

    public:
        int getNumRows() const
        {
            return this->numRows;
        }

        int getNumCols() const
        {
            return this->numCols;
        }

        int getPieceSize() const
        {
            return this->pieceSize;
        }

        void initialize(byte numCols, byte numRows, byte pieceSize)
        {
            this->numCols = numCols;
            this->numRows = numRows;
            this->pieceSize = pieceSize;

            for (int j = 0; j < this->numRows; j ++)
            for (int i = 0; i < this->numCols; i ++)
                this->p[j][i] = 0;
        }

        void moveRowDown(int row)
        {
            for (int r = row; r > 0; r --)
            {
                for (int col = 0; col < this->numCols; col ++)
                    this->p[r][col] = this->p[r - 1][col];
            }

            for (int col = 0; col < this->numCols; col ++)
                this->p[0][col] = 0;
        }

        bool isRowComplete(int row) const
        {
            for (int col = 0; col < this->numCols; col ++)
            {
                if (!this->p[row][col])
                    return false;
            }

            return true;
        }

        void consolidatePiece(int posX, int posY, const TetrisPiece &piece, int value)
        {
            for (int j = 0; j < 5; j ++)
            {
                int cy = j + posY;

                for (int i = 0; i < 5; i ++)
                {
                    int cx = i + posX;

                    if (piece.get(i, j))
                        this->p[cy][cx] = value + 1;
                }
            }
        }

        void consolidatePiece(int posX, int posY, const ColumnsPiece &piece)
        {
            for (int i = 0; i < 3; i ++)
                this->p[posY + i][posX] = piece.get(i) + 1;
        }

        void clearNeighborCells(int cx, int cy, unsigned char v, int &comboSize)
        {
            this->p[cy][cx] = CLEARED_CELL;

            comboSize ++;

            const int ixLs[] = { 0, 0, 1, -1 };
            const int iyLs[] = { 1, -1, 0, 0 };

            for (int i = 0; i < 4; i ++)
            {
                int ix = cx + ixLs[i];
                int iy = cy + iyLs[i];

                if (ix >= 0 && iy >= 0 && ix < this->numCols && iy < this->numRows)
                    if (this->p[iy][ix] == v)
                        this->clearNeighborCells(ix, iy, v, comboSize);
            }
        }

        int clearColumnsCombos()
        {
            int totalComboSize = 0;

            const int ixLs[] = { 0, 0, 1, -1 };
            const int iyLs[] = { 1, -1, 0, 0 };

            for (int cx = 0; cx < this->numCols; cx ++)
            for (int cy = 0; cy < this->numRows; cy ++)
            {
                unsigned char v = this->p[cy][cx];

                if (v != 0 && v != CLEARED_CELL)
                {
                    int n = 0;

                    for (int i = 0; i < 4; i ++)
                    {
                        int ix = cx + ixLs[i];
                        int iy = cy + iyLs[i];

                        if (ix >= 0 && iy >= 0 && ix < this->numCols && iy < this->numRows)
                            if (this->p[iy][ix] == v)
                                n ++;
                    }

                    if (n >= 2)
                    {
                        int comboSize = 0;
                        this->clearNeighborCells(cx, cy, v, comboSize);
                        totalComboSize += comboSize;
                    }
                }
            }

            return totalComboSize;
        }

        void removeClear()
        {
            for (int cx = 0; cx < this->numCols; cx ++)
            for (int cy = 0; cy < this->numRows; cy ++)
            {
                if (this->p[cy][cx] == CLEARED_CELL)
                {
                    for (int k = cy; k > 0; k --)
                        this->p[k][cx] = this->p[k - 1][cx];

                    this->p[0][cx] = 0;
                }
            }
        }

        bool isClear(int cx, int cy) const
        {
            return this->p[cy][cx] == CLEARED_CELL;
        }

        byte get(int cx, int cy) const
        {
            return this->p[cy][cx];
        }
    };

    class GravityGame : public ActivityManager::ActivityLoop
    {
        bool paused;

        const char *bestScoreLabel;

        int bestScore, score;

    protected:
        Board &board;

        Piece &currentPiece;
        int currPosX, currPosY;
        Piece &nextPiece;

        int clearRowState;

        Retarder moveDownRetarder;
        Retarder keyRetarder;
        Retarder keyDownRetarder;
        Retarder clearRowsRetarder;

    public:
        const char *TITLE;

        GravityGame(Board &board, Piece &currentPiece, Piece &nextPiece, const char *title, const char *bestScoreLabel) :
            board            (board),
            currentPiece     (currentPiece),
            nextPiece        (nextPiece),
            TITLE            (title),
            bestScoreLabel   (bestScoreLabel),
            bestScore        (0),
            moveDownRetarder (100),
            keyDownRetarder  ( 30),
            keyRetarder      (150),
            clearRowsRetarder(100)
        {
            PermanentStorage.readBlockInt(bestScoreLabel, this->bestScore);
        }

        virtual void initialize() = 0;

        virtual void initialize(int numCols, int numRows, int pieceSize)
        {
            this->paused = false;

            this->board.initialize(numCols, numRows, pieceSize);
            this->score = 0;

            this->clearRowState = -1;

            this->moveDownRetarder.setTime(1000);

            this->paintBackground();

            this->paintScore();
        }

        virtual bool isPaused() const
        {
            return this->paused;
        }

        virtual void addScore(int points)
        {
            this->score += points;
        }

        virtual int getScore() const
        {
            return this->score;
        }

        virtual int getLevel() const = 0;

        virtual void paintBackground()
        {
            BasicColors::BLACK.EsploraTFT_background();
            SoundManager.background = BasicColors::BLACK;

            EsploraTFT.noFill();
            BasicColors::WHITE.EsploraTFT_stroke();

            int leftLineX  = BOARD_OFFSET_X - 2;
            int rightLineX = BOARD_OFFSET_X + this->board.getNumCols() * this->board.getPieceSize();

            EsploraTFT.line(leftLineX,  0, leftLineX,  DISPLAY_HEIGHT);
            EsploraTFT.line(rightLineX, 0, rightLineX, DISPLAY_HEIGHT);

            EsploraTFT.text("NEXT:",  90, 10);
            EsploraTFT.text("LEVEL:", 90, 60);
            EsploraTFT.text("SCORE:", 90, 80);
            EsploraTFT.text("BEST:",  90, 100);
        }

        virtual void paintFullBoard()
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            for (int j = 0; j < this->board.getNumRows(); j ++)
            {
                int py = BOARD_OFFSET_Y + j * pieceSize;

                for (int i = 0; i < this->board.getNumCols(); i ++)
                {
                    int px = BOARD_OFFSET_X + i * pieceSize;

                    BasicColors::GRAY.EsploraTFT_fill();

                    EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
                }
            }
        }

        virtual void paintBoard() const
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            for (int cy = 0; cy < this->board.getNumRows(); cy ++)
            {
                int py = BOARD_OFFSET_Y + cy * pieceSize;

                for (int cx = 0; cx < this->board.getNumCols(); cx ++)
                {
                    int px = BOARD_OFFSET_X + cx * pieceSize;

                    if (this->board.get(cx, cy))
                        ColorsList[this->board.get(cx, cy) - 1].EsploraTFT_fill();
                    else
                        BasicColors::BLACK.EsploraTFT_fill();

                    EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
                }
            }
        }

        virtual void paintScore() const
        {
            char levelS[10];
            char scoreS[10];
            char bestScoreS[10];

            String(this->getLevel()).toCharArray(levelS, 10);
            String(this->score)     .toCharArray(scoreS, 10);
            String(this->bestScore) .toCharArray(bestScoreS, 10);

            EsploraTFT.noStroke();
            BasicColors::BLACK.EsploraTFT_fill();

            EsploraTFT.rect(140 - 50,  69, 50, 10);
            EsploraTFT.rect(140 - 50,  89, 50, 10);
            EsploraTFT.rect(140 - 50,  109, 50, 10);

            EsploraTFT.noFill();
            BasicColors::WHITE.EsploraTFT_stroke();

            EsploraTFT.text(levelS,     140 - 6 * strlen(levelS), 70);
            EsploraTFT.text(scoreS,     140 - 6 * strlen(scoreS), 90);
            EsploraTFT.text(bestScoreS, 140 - 6 * strlen(bestScoreS), 110);
        }

        virtual void togglePause()
        {
            this->paused = !this->paused;

            if (this->paused)
            {
                this->paintFullBoard();

                this->paintNextPiece(false);

                const char *pauseStr = "PAUSE";
                int pauseStrY = (EsploraTFT.width() - 12 * strlen(pauseStr)) / 2;

                EsploraTFT.textSize(2);
                BasicColors::OLIVE.EsploraTFT_stroke();
                EsploraTFT.text(pauseStr, pauseStrY, 30);
                BasicColors::YELLOW.EsploraTFT_stroke();
                EsploraTFT.text(pauseStr, pauseStrY + 2, 32);
                EsploraTFT.textSize(1);
            }
            else
            {
                this->paintBackground();

                this->paintScore();

                this->paintBoard();

                this->paintNextPiece(true);

                this->paintPiece(true);
            }
        }

        virtual void paintNextPiece(bool set = true) const = 0;

        virtual void paintPiece(bool set = true) const = 0;

        virtual bool tryPiece() const = 0;

        virtual void consolidatePiece() = 0;

        void tryMovePiece(int dir)
        {
            this->paintPiece(false);

            this->currPosX += dir;

            if (this->tryPiece())
                SoundManager.playFXSound(4400, 10);
            else
                this->currPosX -= dir;

            this->paintPiece(true);
        }

        void tryRotatePiece(bool dir)
        {
            this->paintPiece(false);

            this->currentPiece.rotate(dir);

            if (this->tryPiece())
                SoundManager.playFXSound(3300, 10);
            else
                this->currentPiece.rotate(!dir);

            this->paintPiece(true);
        }

        void movePieceDown()
        {
            this->paintPiece(false);

            this->currPosY ++;

            if (this->tryPiece())
            {
                SoundManager.playFXSound(2200, 10);
            }
            else
            {
                this->currPosY --;

                this->paintPiece();

                this->consolidatePiece();
            }

            this->paintPiece(true);
        }
        
        virtual void completeClear() = 0;

        virtual void paintClearRows(bool set = true) const = 0;

        virtual void loop()
        {
            if (SwitchesManager.isPressed(SWITCH_LEFT))
                this->togglePause();

            if (this->clearRowState == -1)
            {
                if (!this->isPaused())
                {
                    if (this->moveDownRetarder.expired())
                        this->movePieceDown();

                    else
                    {
                        int joystickX = Esplora.readJoystickX();
                        int joystickY = Esplora.readJoystickY();

                        if (joystickY > abs(joystickX))
                        {
                            if (joystickY > 200 && this->keyDownRetarder.expired())
                                this->movePieceDown();
                        }
                        else
                        {
                            if (joystickX < -200 && this->keyRetarder.expired())
                                this->tryMovePiece(1);

                            if (joystickX > 200 && this->keyRetarder.expired())
                                this->tryMovePiece(-1);
                        }

                        if (SwitchesManager.isPressed(SWITCH_DOWN))
                            this->tryRotatePiece(true);

                        else if (SwitchesManager.isPressed(SWITCH_RIGHT))
                            this->tryRotatePiece(false);
                    }
                }
            }

            else if (this->clearRowsRetarder.expired())
            {
                this->clearRowState ++;

                if (this->clearRowState == 5)
                {
                    this->completeClear();
                }
                else
                {
                    if (!(this->clearRowState & 1))
                        SoundManager.playFXSound(6000, 50);

                    this->paintClearRows(this->clearRowState & 1);
                }
            }
        }

        virtual void gameOver()
        {
            if (this->score > this->bestScore)
            {
                this->bestScore = this->score;

                PermanentStorage.writeBlockInt(bestScoreLabel, this->bestScore);
            }

            ActivityManager::SetGameActivity(ActivityManager::GAME_OVER);
        }
    };

    const TetrisPiece TetrisPiecesList[] =
    {
        TetrisPiece(
                "     "
                "     "
                "  *X "
                "  XX "
                "     "),
        TetrisPiece(
                "     "
                "     "
                " X*  "
                "  XX "
                "     "),
        TetrisPiece(
                "     "
                "     "
                " X*XX"
                "     "
                "     "),
        TetrisPiece(
                "     "
                "     "
                " X*X "
                "   X "
                "     "),
        TetrisPiece(
                "     "
                "     "
                "  *X "
                " XX  "
                "     "),
        TetrisPiece(
                "     "
                "     "
                " X*X "
                "  X  "
                "     "),
        TetrisPiece(
                "     "
                "     "
                " X*X "
                " X   "
                "     ")
    };

    enum { NUM_TETRIS_PIECES = 7 };

    class TetrisGame : public GravityGame
    {
        int current, next;
        TetrisPiece currentPiece, nextPiece;

        int clearRows[4];
        int numClearRows;

    public:
        TetrisGame(Board &board) :
            GravityGame(board, this->currentPiece, this->nextPiece, "TETRISDUINO", "Te.Best")
        {
        }

        void initialize()
        {
            GravityGame::initialize(9, 21, 6);

            this->next = random(NUM_TETRIS_PIECES);
            this->nextPiece = TetrisPiecesList[this->next];

            this->numClearRows = 0;

            this->createPiece();
        }

        virtual int getLevel() const
        {
            int level = 1 + this->getScore() / 20;

            if (level > 10)
                level = 10;

            return level;
        }

        bool tryPiece() const
        {
            for (int j = 0; j < 5; j ++)
            {
                int cy = j + this->currPosY;

                for (int i = 0; i < 5; i ++)
                {
                    int cx = i + this->currPosX;

                    if (this->currentPiece.get(i, j))
                    {
                        if (cx < 0 || cy < 0 || cx >= this->board.getNumCols() || cy >= this->board.getNumRows() || this->board.get(cx, cy))
                            return false;
                    }
                }
            }

            return true;
        }

        void paintPiece(const TetrisPiece &piece, const Color &color, int x, int y, bool set, bool clearBg) const
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            for (int j = 0; j < 5; j ++)
            {
                int py = y + j * pieceSize;

                for (int i = 0; i < 5; i ++)
                {
                    int px = x + i * pieceSize;

                    bool isCell = piece.get(i, j);

                    if (clearBg || isCell)
                    {
                        if (isCell && set)
                            color.EsploraTFT_fill();
                        else
                            BasicColors::BLACK.EsploraTFT_fill();

                        EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
                    }
                }
            }
        }

        void paintPiece(bool set = true) const
        {
            const int pieceSize = this->board.getPieceSize();

            this->paintPiece(this->currentPiece, ColorsList[this->current],
                             BOARD_OFFSET_X + this->currPosX * pieceSize,
                             BOARD_OFFSET_Y + this->currPosY * pieceSize, set, false);
        }

        void paintNextPiece(bool set = true) const
        {
            this->paintPiece(this->nextPiece, ColorsList[this->next],
                             NEXT_PIECE_OFFSET_X,
                             NEXT_PIECE_OFFSET_Y, set, true);
        }

        bool createPiece()
        {
            this->current      = this->next;
            this->currentPiece = this->nextPiece;

            this->currPosX = 2;
            this->currPosY = -2;

            while (this->currPosY < 0 && !this->tryPiece())
                this->currPosY ++;

            if (!this->tryPiece())
            {
                return false;
            }
            else
            {
                this->next = random(NUM_TETRIS_PIECES);
                this->nextPiece = TetrisPiecesList[this->next];

                int randRot = random(4);

                for (int i = 0; i < randRot; i ++)
                    this->nextPiece.rotate(true);

                this->paintNextPiece(true);

                this->paintPiece(true);

                return true;
            }
        }

        void consolidatePiece()
        {
            this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece, this->current);

            this->numClearRows = 0;

            for (int row = 0; row < this->board.getNumRows(); row ++)
            {
                if (this->board.isRowComplete(row))
                {
                    this->board.moveRowDown(row);

                    this->clearRows[this->numClearRows] = row;
                    this->numClearRows ++;

                    this->addScore(1);
                }
            }

            if (this->numClearRows > 0)
            {
                SoundManager.playFXSound(6000, 50);

                this->paintClearRows(false);

                this->clearRowState = 0;
                this->clearRowsRetarder.restart();
            }
            else
            {
                if (!this->createPiece())
                {
                    this->paintNextPiece(false);

                    this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece, this->current);

                    this->gameOver();
                }

                this->moveDownRetarder.restart();
                this->keyDownRetarder.restart(150);
            }
        }

        void paintClearRows(bool set) const
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            if (set)
                BasicColors::WHITE.EsploraTFT_fill();
            else
                BasicColors::BLACK.EsploraTFT_fill();

            for (int k = 0; k < this->numClearRows; k ++)
            {
                int py = BOARD_OFFSET_Y + this->clearRows[k] * pieceSize;

                int px = BOARD_OFFSET_X;

                for (int i = 0; i < this->board.getNumCols(); i ++)
                {
                    EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);

                    px += pieceSize;
                }
            }
        }
        
        void completeClear()
        {
            this->clearRowState = -1;

            if (!this->createPiece())
            {
                this->paintNextPiece(false);

                this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece, this->current);

                this->gameOver();
            }
            else
            {
                this->paintBoard();
                this->paintPiece();
                this->paintScore();

                this->moveDownRetarder.setTime(1000 - 90 * this->getLevel());
                this->keyDownRetarder.restart(150);
            }
        }
    };

    class ColumnsGame : public GravityGame
    {
        ColumnsPiece currentPiece, nextPiece;

        int scoreFactor;

    public:
        ColumnsGame(Board &board) :
            GravityGame(board, this->currentPiece, this->nextPiece, "COLUMNSDUINO", "Co.Best")
        {
        }

        void initialize()
        {
            GravityGame::initialize(6, 14, 9);

            this->createPiece();
        }

        virtual int getLevel() const
        {
            int level = 1 + this->getScore() / 100;

            if (level > 10)
                level = 10;

            return level;
        }

        void paintPiece(const ColumnsPiece &piece, int x, int y, bool set = true) const
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            for (int i = 0; i < 3; i ++)
            {
                int px = x;
                int py = y + i * pieceSize;

                if (set)
                    ColorsList[piece.get(i)].EsploraTFT_fill();
                else
                    BasicColors::BLACK.EsploraTFT_fill();

                EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
            }
        }

        void paintPiece(bool set = true) const
        {
            const int pieceSize = this->board.getPieceSize();

            this->paintPiece(this->currentPiece,
                             BOARD_OFFSET_X + currPosX * pieceSize,
                             BOARD_OFFSET_Y + currPosY * pieceSize, set);
        }

        void paintNextPiece(bool set = true) const
        {
            const int pieceSize = this->board.getPieceSize();

            this->paintPiece(this->nextPiece,
                             NEXT_PIECE_OFFSET_X + 2 * pieceSize,
                             NEXT_PIECE_OFFSET_Y, set);
        }

        bool createPiece()
        {
            this->currentPiece = this->nextPiece;

            this->currPosX = 2;
            this->currPosY = 0;

            while (this->currPosY < 0 && !this->tryPiece())
                this->currPosY ++;

            if (!this->tryPiece())
            {
                return false;
            }
            else
            {
                this->nextPiece = ColumnsPiece();

                this->paintPiece(true);

                this->paintNextPiece(true);

                return true;
            }
        }

        virtual void paintClearRows(bool set = true) const
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            if (set)
                BasicColors::WHITE.EsploraTFT_fill();
            else
                BasicColors::BLACK.EsploraTFT_fill();

            for (int cy = 0; cy < this->board.getNumRows(); cy ++)
            {
                int py = BOARD_OFFSET_Y + cy * pieceSize;

                for (int cx = 0; cx < this->board.getNumCols(); cx ++)
                {
                    int px = BOARD_OFFSET_X + cx * pieceSize;

                    if (this->board.isClear(cx, cy))
                        EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
                }
            }
        }

        void consolidatePiece()
        {
            this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece);

            int comboSize = this->board.clearColumnsCombos();

            if (comboSize > 0)
            {
                this->scoreFactor = 1;
                this->addScore(this->scoreFactor * comboSize);

                this->clearRowState = 0;
                this->clearRowsRetarder.restart();
            }
            else
            {
                if (!this->createPiece())
                {
                    this->paintNextPiece(false);

                    this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece);

                    this->gameOver();
                }

                this->moveDownRetarder.restart();
                this->keyDownRetarder.restart(150);
            }
        }

        bool tryPiece() const
        {
            for (int i = 0; i < 3; i ++)
            {
                int cy = i + this->currPosY;

                if (this->currPosX < 0 || cy < 0 ||
                    this->currPosX >= this->board.getNumCols() || cy >= this->board.getNumRows() ||
                    this->board.get(this->currPosX, cy))
                {
                    return false;
                }
            }

            return true;
        }
        
        void completeClear()
        {
            this->board.removeClear();
            this->paintBoard();

            int comboSize = this->board.clearColumnsCombos();

            if (comboSize > 0)
            {
                this->scoreFactor ++;
                this->addScore(this->scoreFactor * comboSize);
                this->clearRowState = 0;
            }
            else
            {
                this->clearRowState = -1;

                if (!this->createPiece())
                {
                    this->paintNextPiece(false);

                    this->board.consolidatePiece(this->currPosX, this->currPosY, this->currentPiece);

                    this->gameOver();
                }
                else
                {
                    this->paintBoard();
                    this->paintPiece();
                    this->paintScore();

                    this->moveDownRetarder.setTime(1000 - 70 * this->getLevel());
                    this->keyDownRetarder.restart(150);
                }
            }
        }
    };

    const char *tetrisStr = "Play TETRIS";
    const char *columnsStr = "Play COLUMNS";

    const char *tetrisSelectedStr = ">> Play TETRIS <<";
    const char *columnsSelectedStr = ">> Play COLUMNS <<";

    class GameStart
    {
        Retarder blinkRetarder;

        bool blink;

        int selected;

        char const *gameTitle;

    public:
        GameStart() : blinkRetarder(300), blink(false), selected(0), gameTitle("TETRISDUINO")
        {
        }

        void setTitle(const char *gameTitle)
        {
            this->gameTitle = gameTitle;
        }

        void initialize()
        {
            BasicColors::BLUE.EsploraTFT_background();
            SoundManager.background = BasicColors::BLUE;

            if (gameTitle)
            {
                int gameStrX = (EsploraTFT.width() - 12 * strlen(gameTitle)) / 2;

                EsploraTFT.noFill();

                EsploraTFT.textSize(2);
                BasicColors::LIGHT_GRAY.EsploraTFT_stroke();
                EsploraTFT.text(gameTitle, gameStrX, 30);
                BasicColors::WHITE.EsploraTFT_stroke();
                EsploraTFT.text(gameTitle, gameStrX + 2, 32);
                EsploraTFT.textSize(1);
            }

            const char *authorStr = "by Ismael Aguilera";
            EsploraTFT.text(authorStr,  (EsploraTFT.width() - 6 * strlen(authorStr)) / 2, 50);

            this->paintOptions();
        }

        void paintOptions()
        {
            EsploraTFT.noStroke();
            BasicColors::BLUE.EsploraTFT_fill();

            const int EsploraTFT_width = EsploraTFT.width();

            if (this->selected == 1)
                EsploraTFT.rect(0, 80, EsploraTFT_width, 8);

            if (this->selected == 0)
                EsploraTFT.rect(0, 95, EsploraTFT_width, 8);

            EsploraTFT.noFill();
            BasicColors::WHITE.EsploraTFT_stroke();

            if (this->selected == 1)
                EsploraTFT.text(tetrisStr,  (EsploraTFT_width - 6 * strlen(tetrisStr)) / 2, 80);

            if (this->selected == 0)
                EsploraTFT.text(columnsStr, (EsploraTFT_width - 6 * strlen(columnsStr)) / 2, 95);

            this->toggleBlink();
        }

        void toggleBlink()
        {
            EsploraTFT.noFill();

            if (this->blink)
                BasicColors::WHITE.EsploraTFT_stroke();
            else
                BasicColors::YELLOW.EsploraTFT_stroke();

            if (this->selected == 0)
                EsploraTFT.text(tetrisSelectedStr,  (EsploraTFT.width() - 6 * strlen(tetrisSelectedStr)) / 2, 80);

            if (this->selected == 1)
                EsploraTFT.text(columnsSelectedStr, (EsploraTFT.width() - 6 * strlen(columnsSelectedStr)) / 2, 95);
        }

        void loop()
        {
            if (blinkRetarder.expired())
            {
                this->blink = !this->blink;

                this->toggleBlink();
            }

            if (this->selected != 1 && Esplora.readJoystickY() > 250)
            {
                SoundManager.playFXSound(4400, 10);

                this->selected = 1;
                this->blink = true;
                this->paintOptions();
            }

            if (this->selected != 0 && Esplora.readJoystickY() < -250)
            {
                SoundManager.playFXSound(4400, 10);

                this->selected = 0;
                this->blink = true;
                this->paintOptions();
            }

            if (SwitchesManager.isPressed(SWITCH_DOWN))
            {
                if (this->selected == 0)
                    ActivityManager::StartTetris();

                if (this->selected == 1)
                    ActivityManager::StartColumns();
            }
        }
    };

    class GameOver
    {
        const Board &board;

        int blocksToPaint;

        Retarder fillBoardRetarder;

        void paintFullBoard()
        {
            const int pieceSize = this->board.getPieceSize();

            EsploraTFT.noStroke();

            int numBlocks = 0;

            BasicColors::GRAY.EsploraTFT_fill();

            for (int cy = 0; cy < this->board.getNumRows(); cy ++)
            {
                for (int cx = this->board.getNumCols() - 1; cx >= 0; cx --)
                {
                    if (numBlocks == this->blocksToPaint && this->board.get(cx, cy))
                    {
                        int px = BOARD_OFFSET_X + cx * pieceSize;
                        int py = BOARD_OFFSET_Y + cy * pieceSize;

                        EsploraTFT.rect(px, py, pieceSize - 1, pieceSize - 1);
                    }

                    numBlocks ++;
                }
            }
        }

        void paintGameOver(bool alsoShadow)
        {
            const char *gameOverStr = "GAME OVER";

            EsploraTFT.textSize(2);

            int gameOverStrX = (EsploraTFT.width() - 12 * strlen(gameOverStr)) / 2;

            if (alsoShadow)
            {
                BasicColors::OLIVE.EsploraTFT_stroke();
                EsploraTFT.text(gameOverStr, gameOverStrX, 30);
            }

            BasicColors::YELLOW.EsploraTFT_stroke();
            EsploraTFT.text(gameOverStr, gameOverStrX + 2, 32);
            EsploraTFT.textSize(1);
        }

    public:
        GameOver(Board &board) : fillBoardRetarder(30), board(board)
        {
        }

        void initialize()
        {
            this->blocksToPaint = this->board.getNumCols() * this->board.getNumRows();

            this->paintGameOver(true);
        }

        void loop()
        {
            if (this->blocksToPaint > 0 && fillBoardRetarder.expired())
            {
                if ((this->blocksToPaint % this->board.getNumCols()) == 0)
                    SoundManager.playFXSound(3000, 20);

                this->blocksToPaint --;

                this->paintFullBoard();

                this->paintGameOver(false);
            }
            else
            {
                Esplora.noTone();
            }

            if (SwitchesManager.isPressed(SWITCH_DOWN))
                ActivityManager::SetGameActivity(ActivityManager::GAME_START);
        }
    };
}

namespace ActivityManager
{
    BoardGame::GameStart gameStart;

    BoardGame::Board board;

    BoardGame::TetrisGame tetrisGame(board);

    BoardGame::ColumnsGame columnsGame(board);

    ActivityLoop *game = &tetrisGame;

    BoardGame::GameOver gameOver(board);

    int activity = 0;

    void StartTetris()
    {
        game = &tetrisGame;

        gameStart.setTitle(tetrisGame.TITLE);

        SetGameActivity(ONGOING_GAME);
    }

    void StartColumns()
    {
        game = &columnsGame;

        gameStart.setTitle(columnsGame.TITLE);

        SetGameActivity(ONGOING_GAME);
    }

    void SetGameActivity(int _activity)
    {
        activity = _activity;

        Esplora.noTone();

        if (activity == GAME_START)
            gameStart.initialize();

        else if (activity == ONGOING_GAME)
            game->initialize();

        else if (activity == GAME_OVER)
            gameOver.initialize();
    }

    void Loop()
    {
        SoundManager.loop();

        if (activity == GAME_START)
            gameStart.loop();

        else if (activity == ONGOING_GAME)
            game->loop();

        else if (activity == GAME_OVER)
            gameOver.loop();
    }
}

void setup()
{
    EsploraTFT.begin();

    ActivityManager::SetGameActivity(ActivityManager::GAME_START);
}

void loop()
{
    ActivityManager::Loop();
}

