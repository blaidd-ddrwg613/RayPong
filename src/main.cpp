#include <cmath>
#include <string>
#include "raylib.h"
#include "raymath.h"

// ---------------------------------------------------------------------------
// Global settings and game state
// ---------------------------------------------------------------------------

/// TODO Fix aiPaddle stuttering not sure what is causing it, could be the tracking and dead zone?
/// TODO Add Game Over State instead of reseting the game right away
/// TODO Nice to have a start menue

// Starting window size. Updated when the window is resized.
int WindowWidth = 800;
int WindowHeight = 600;
std::string WindowTitle = "RayPong";

// AI skill from 0.0 (easiest) to 1.0 (hardest). Changed with the arrow keys.
float Difficulty = 0.5f;

int PlayerScore{0};
int AiScore{0};

// ---------------------------------------------------------------------------
// Paddle: used for both the player (left) and the AI (right)
// ---------------------------------------------------------------------------
struct Paddle
{
    float width = 10;
    float height = 80;
    float speed = 400.0f;       // Max movement speed in pixels per second
    Vector2 Position = {0, 0};  // Top-left corner

    // Move the paddle with W/S or Up/Down.
    void UpdatePlayer(float dt)
    {
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
            Position.y -= speed * dt;

        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
            Position.y += speed * dt;

        ClampToScreen();
    }

    // Move the paddle toward targetY (usually the ball's height).
    // shouldTrack: only move while the ball is heading toward this paddle.
    // difficulty:  0..1, higher means a smaller dead zone and faster tracking.
    void UpdateAI(float dt, float targetY, bool shouldTrack, float difficulty)
    {
        if (!shouldTrack)
            return;

        float paddleCenter = Position.y + height * 0.5f;

        // Don't move if the target is already close to the paddle's center.
        // Easy AI ignores small gaps (30px), hard AI reacts to almost any gap (2px).
        float deadZone = Lerp(30.0f, 2.0f, difficulty);

        // How far the paddle center is from where it wants to be
        float error = targetY - paddleCenter;

        if (fabsf(error) < deadZone)
            return;

        // Easy AI moves at 30% of max speed, hard AI at 100%.
        float trackingSpeed = speed * Lerp(0.3f, 1.0f, difficulty);

        // Move faster the further away the target is, capped at trackingSpeed.
        // This makes the AI slow down as it lines up, instead of jittering.
        float movement = Clamp(error * 5.0f, -trackingSpeed, trackingSpeed);

        Position.y += movement * dt;

        ClampToScreen();
    }

    // Stop the paddle from going past the top or bottom of the window.
    void ClampToScreen()
    {
        if (Position.y < 0)
            Position.y = 0;

        if (Position.y > GetScreenHeight() - height)
            Position.y = GetScreenHeight() - height;
    }

    // Paddle bounds, used for drawing and collision checks.
    Rectangle GetRect() const
    {
        return {Position.x, Position.y, width, height};
    }

    void Draw()
    {
        DrawRectangleRec(GetRect(), WHITE);
    }
};

// ---------------------------------------------------------------------------
// Ball
// ---------------------------------------------------------------------------
struct Ball
{
    Vector2 Position = {};  // Center of the ball
    Vector2 Velocity = {};  // Direction and speed in pixels per second

    float Speed = 300.0f;   // Current speed. Goes up with each paddle hit.
    float Radius = 10.0f;

    // Move the ball, then handle scoring, wall bounces and paddle hits.
    void Update(float dt, Paddle& leftPaddle, Paddle& rightPaddle)
    {
        Position.x += Velocity.x * dt;
        Position.y += Velocity.y * dt;

        // Scoring: the ball fully left the screen on one side.
        // Past the left edge, the AI scores.
        if (Position.x + Radius < 0)
        {
            AiScore++;
            Reset();
        }

        // Past the right edge, the player scores.
        if (Position.x - Radius > GetScreenWidth())
        {
            PlayerScore++;
            Reset();
        }

        // Top wall: push the ball back inside and flip its vertical direction.
        if (Position.y - Radius <= 0)
        {
            Position.y = Radius;
            Velocity.y *= -1;
        }

        // Bottom wall: same as the top wall.
        if (Position.y + Radius >= GetScreenHeight())
        {
            Position.y = GetScreenHeight() - Radius;
            Velocity.y *= -1;
        }

        // Left (player) paddle.
        // Only check while the ball moves left, so it can't hit the same paddle twice in a row.
        if (Velocity.x < 0 && CheckCollisionCircleRec(Position, Radius, leftPaddle.GetRect()))
        {
            float paddleCenter = leftPaddle.Position.y + leftPaddle.height * 0.5f;

            // Where on the paddle the ball hit: -1 = top edge, 0 = center, 1 = bottom edge.
            float distance = Position.y - paddleCenter;
            float normalized = Clamp(distance / (leftPaddle.height * 0.5f), -1.0f, 1.0f);

            // Send the ball right. Hits near the edges bounce off at a steeper angle.
            Velocity.x = fabsf(Velocity.x);
            Velocity.y = normalized * Speed;

            // Speed up a little on every hit, up to a max of 1000.
            Speed += 10;
            Speed = Clamp(Speed, 300.0f, 1000.0f);

            // Keep the new direction but set its length to the current speed.
            Velocity = Vector2Scale(Vector2Normalize(Velocity), Speed);

            // Prevent near-vertical movement: make sure at least 30% of the
            // speed goes sideways so the ball keeps crossing the screen.
            if (fabsf(Velocity.x) < Speed * 0.3f)
            {
                Velocity.x = Speed * 0.3f;

                Velocity = Vector2Scale(Vector2Normalize(Velocity), Speed);
            }

            // Move the ball out of the paddle so it doesn't get stuck inside.
            Position.x = leftPaddle.Position.x + leftPaddle.width + Radius;
        }

        // Right (AI) paddle. Same logic as the left paddle, but the ball goes left.
        if (Velocity.x > 0 &&
            CheckCollisionCircleRec(Position, Radius, rightPaddle.GetRect()))
        {
            float paddleCenter = rightPaddle.Position.y + rightPaddle.height * 0.5f;

            // Where on the paddle the ball hit: -1 = top edge, 0 = center, 1 = bottom edge.
            float distance = Position.y - paddleCenter;
            float normalized = Clamp(distance / (rightPaddle.height * 0.5f), -1.0f, 1.0f);

            // Send the ball left, angled by where it hit.
            Velocity.x = -fabsf(Velocity.x);
            Velocity.y = normalized * Speed;

            // Speed up a little on every hit, up to a max of 1000.
            Speed += 10;
            Speed = Clamp(Speed, 300.0f, 1000.0f);

            Velocity = Vector2Scale(Vector2Normalize(Velocity), Speed);

            // Prevent near-vertical movement (see left paddle).
            if (fabsf(Velocity.x) < Speed * 0.3f)
            {
                Velocity.x = -Speed * 0.3f;

                Velocity = Vector2Scale(Vector2Normalize(Velocity), Speed);
            }

            // Move the ball out of the paddle so it doesn't get stuck inside.
            Position.x = rightPaddle.Position.x - Radius;
        }
    }

    // Put the ball back in the center and serve it in a random direction.
    void Reset()
    {
        Position =
        {
            GetScreenWidth() * 0.5f,
            GetScreenHeight() * 0.5f
        };

        Speed = 300.0f;

        // Randomly serve left or right...
        Velocity.x = (GetRandomValue(0, 1) ? 1.0f : -1.0f);

        // ...with a random slope between -0.75 and 0.75.
        Velocity.y = GetRandomValue(-75, 75) / 100.0f;

        // Turn that direction into a velocity at the starting speed.
        Velocity = Vector2Scale(Vector2Normalize(Velocity), Speed);
    }

    void Draw()
    {
        DrawCircleV(Position, Radius, WHITE);
    }
};

// Start a new match: clear scores, center both paddles and serve a new ball.
void ResetGame(
    Ball& ball,
    Paddle& player,
    Paddle& ai)
{
    PlayerScore = 0;
    AiScore = 0;

    player.Position.y =
        (GetScreenHeight() - player.height) * 0.5f;

    ai.Position.y =
        (GetScreenHeight() - ai.height) * 0.5f;

    ball.Reset();
}

int main()
{
    // Window flags must be set before the window is created.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WindowWidth, WindowHeight, WindowTitle.c_str());

    // Player paddle: 10px from the left edge, vertically centered.
    Paddle playerPaddle;
    playerPaddle.Position =
    {
        10,
        (GetScreenHeight() - playerPaddle.height) * 0.5f
    };

    // AI paddle: 10px from the right edge, vertically centered.
    Paddle aiPaddle;
    aiPaddle.Position =
    {
        GetScreenWidth() - aiPaddle.width - 10,
        (GetScreenHeight() - aiPaddle.height) * 0.5f
    };

    Ball ball;
    ball.Reset();

    SetTargetFPS(60);

    // Main game loop: runs once per frame until the window is closed.
    while (!WindowShouldClose())
    {
        // Seconds since the last frame. Used so movement speed doesn't depend on FPS.
        float dt = GetFrameTime();

        // ---------------- Update ----------------

        // Keep the AI paddle against the right edge when the window size changes.
        if (IsWindowResized())
        {
            WindowWidth = GetScreenWidth();
            WindowHeight = GetScreenHeight();

            aiPaddle.Position.x = GetScreenWidth() - aiPaddle.width - 10;
        }

        // Left/Right arrows change the AI difficulty in 10% steps.
        if (IsKeyPressed(KEY_LEFT))
        {
            Difficulty -= 0.1f;
        }

        if (IsKeyPressed(KEY_RIGHT))
        {
            Difficulty += 0.1f;
        }

        // Start a new game when someone reaches 10 points, or when R is pressed.
        if (PlayerScore >= 10 || AiScore >= 10 || IsKeyPressed(KEY_R)) ResetGame(ball, playerPaddle, aiPaddle);

        // Keep difficulty between 0% and 100%.
        Difficulty = Clamp(Difficulty, 0.0f, 1.0f);

        playerPaddle.UpdatePlayer(dt);

        // AI aims at the ball, plus random error that shrinks as difficulty goes up
        // (up to +/-100px at 0% difficulty, none at 100%).
        float targetY = ball.Position.y;
        targetY += GetRandomValue(-100, 100) * (1.0f - Difficulty);
        // The AI only moves while the ball is heading toward it (moving right).
        aiPaddle.UpdateAI(dt, targetY,ball.Velocity.x > 0.0f, Difficulty);

        ball.Update(dt, playerPaddle, aiPaddle);

        // ---------------- Draw ----------------
        BeginDrawing();

        ClearBackground(SKYBLUE);

        // Dashed center line: 15px dashes with 15px gaps.
        for (int y = 0; y < GetScreenHeight(); y += 30)
        {
            DrawLine(GetScreenWidth() / 2,y,GetScreenWidth() / 2,y + 15,LIGHTGRAY);
        }

        // Difficulty and controls in the top-left corner.
        DrawText(TextFormat("AI Difficulty: %.0f%%", Difficulty * 100.0f), 10, 10, 20, WHITE);
        DrawText("Left Arrow to Decrease Difficulty", 10, 40, 15, WHITE);
        DrawText("Right Arrow to Increase Difficulty", 10, 60, 15, WHITE);
        DrawText("Press R to Reset Game", 10, 80, 15, WHITE);

        // Scores on either side of the center line.
        std::string pScoreText = std::to_string(PlayerScore);
        std::string aScoreText = std::to_string(AiScore);
        // Measure the player's score so it can end 20px left of center.
        int leftWidth = MeasureText(pScoreText.c_str(),40);
        DrawText(pScoreText.c_str(),GetScreenWidth()/2 - 20 - leftWidth,20,40,WHITE);
        // The AI's score starts 20px right of center.
        DrawText(aScoreText.c_str(), GetScreenWidth() / 2 + 20, 20, 40, WHITE);

        playerPaddle.Draw();
        aiPaddle.Draw();
        ball.Draw();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
