#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <deque>

#define SCREEN_WIDTH 128 // OLED width,  in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels

// create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define HEARTBEAT_PIN 23
#define HEARTBEAT_PERIOD 1000

#define PIN_UP 5
#define PIN_DOWN 2
#define PIN_RIGHT 4
#define PIN_LEFT 15

enum Button
{
    NONE = 0,
    UP = PIN_UP,
    DOWN = PIN_DOWN,
    LEFT = PIN_LEFT,
    RIGHT = PIN_RIGHT
};

uint8_t buttons[] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT};
auto direction = Button::NONE;

void keyChangeISR()
{
    if(!digitalRead(PIN_UP))
        direction = Button::UP;
    
    if(!digitalRead(PIN_DOWN))
        direction = Button::DOWN;

    if(!digitalRead(PIN_LEFT))
        direction = Button::LEFT;

    if(!digitalRead(PIN_RIGHT))
        direction = Button::RIGHT;
}

struct Point{
    int16_t x;
    int16_t y;

    Point(int16_t _x, int16_t _y) :
        x(_x),
        y(_y)
    {}

    Point(const Point &point) :
        x(point.x),
        y(point.y)
    {}

    void set(const Point &point){
        x = point.x;
        y = point.y;
    }

    void set(int16_t _x, int16_t _y){
        x = _x;
        y = _y;
    }

    void move(int16_t dx, int16_t dy){
        x += dx;
        y += dy;
    }

    void move(const Point &point){
        x += point.x;
        y += point.y;
    }

    bool operator== (const Point& point){
        return (point.x == x) && (point.y == y);
    }

    bool operator!= (const Point& point){
        return (point.x != x) || (point.y != y);
    }

    Point operator+ (const Point& point){
        Point result(point);
        result.move(*this);
        return result;
    }
    
    Point operator- (const Point& point){
        return operator+(point);
    }
};

#define SNAKE_BODY_LEN 10
#define SNAKE_SPEED 1
#define SNAKE_GROW_PER_APPLE 10

struct Snake{
    Adafruit_SSD1306 &_oled;
    std::deque<Point> _body;
    Button _direction;
    Point _speed;
    bool _dead;
    Point _apple;
    uint8_t _extra_length;

    Snake(Adafruit_SSD1306 &oled) :
        _oled(oled),
        _direction(Button::NONE),
        _speed(0,0),
        _dead(false),
        _apple(-1,-1),
        _extra_length(0)
    {
        Point head(SCREEN_HEIGHT/2, SCREEN_WIDTH/2);
        _body.push_back(head);
        for( int i=0; i<SNAKE_BODY_LEN; i++){
            auto prev_segment = _body.back(); 
            Point segment(prev_segment.x+1, prev_segment.y);
            _body.push_back(segment);
        }
    }

    void begin(){
        _oled.setCursor(0,0);
        _oled.println("Press any!");
        // draw bounding box
        _oled.drawRect(oled.getCursorX(), oled.getCursorY(), 
                      SCREEN_HEIGHT, SCREEN_WIDTH - oled.getCursorY(), 1);

        // draw worm
        for(auto segment : _body){
            _oled.drawPixel(segment.x, segment.y, 1);
        }

        // display initial screen
        _oled.display();
    }

    void tick(Button direction, unsigned long ticks){
        //oled.clearDisplay();
        // draw score
        _oled.setCursor(0,0);
        _oled.println(_body.size()*100); // set te

        if(_dead){
            return;
        }

        handleApple();

        buttonHandler(direction);

        move();

        _oled.display();
    }

    void handleApple(){
        // if no apple spawn an apple
        if(_apple == Point(-1,-1)){
            do{
                _apple.x = random(oled.getCursorX()+1,SCREEN_HEIGHT-1);
                _apple.y = random(oled.getCursorY()+1, SCREEN_WIDTH - oled.getCursorY() -1);
            } while(true == _oled.getPixel(_apple.x, _apple.y));
        }

        _oled.drawPixel(_apple.x, _apple.y, 1);
    }

    void buttonHandler(Button dir){
        Point new_speed(0,0);
        switch (direction)
        {
            case Button::UP:
                new_speed.set(0, -SNAKE_SPEED);
                break;
            case Button::DOWN:
                new_speed.set(0, SNAKE_SPEED);
                break;
            case Button::LEFT:
                new_speed.set(-SNAKE_SPEED, 0);
                break;
            case Button::RIGHT:
                if(_speed != Point(0,0))
                    new_speed.set(SNAKE_SPEED, 0);
                break;
            default:
                break;
        }

        if((_speed-new_speed) == Point(0,0)){
            return;
        }

        _speed.set(new_speed);
    }

    void move(){
        // don't do anything of speed is 0
        if(_speed == Point(0,0)){
            return;
        }

        // get head and tail of snake
        Point tail = _body.back(); 
        Point new_head(_body.front());

        // check if head runs into something
        new_head.move(_speed);
        if(true == _oled.getPixel(new_head.x, new_head.y))
        {
            if(new_head == _apple){
                _extra_length = SNAKE_GROW_PER_APPLE;
                _apple.set(Point(-1,-1));
            } else if(new_head == tail) {
                // purposely left blank
            } else {
                _oled.println("");
                _oled.println("   DEAD!   ");
                _dead = true;
                return;
            }
        }

        // delete tail
        _oled.writePixel(tail.x, tail.y, 0);
        
        // add new head
        _body.push_front(new_head);
        _oled.writePixel(_body.front().x, _body.front().y, 1);
        
        // if snake can grow exit early
        if(_extra_length--){
            return; 
        }

        // remove last piece of tail
        _body.pop_back();
    }
};

extern "C" void app_main()
{
    initArduino();
    pinMode(HEARTBEAT_PIN, OUTPUT);

    pinMode(PIN_UP, INPUT_PULLUP);
    attachInterrupt(PIN_UP, keyChangeISR, FALLING);

    pinMode(PIN_DOWN, INPUT_PULLUP);
    attachInterrupt(PIN_DOWN, keyChangeISR, FALLING);

    pinMode(PIN_LEFT, INPUT_PULLUP);
    attachInterrupt(PIN_LEFT, keyChangeISR, FALLING);

    pinMode(PIN_RIGHT, INPUT_PULLUP);
    attachInterrupt(PIN_RIGHT, keyChangeISR, FALLING);

    Serial.begin(9600);

    // initialize OLED display with I2C address 0x3C
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("failed to start SSD1306 OLED"));
        while (1);
    }

    delay(2000);         // wait two seconds for initializing
    oled.setRotation(1);
    oled.clearDisplay(); // clear display

    oled.setTextSize(1);         // set text size
    oled.setTextColor(WHITE);    // set text color
    oled.display();  
    
    Snake snake(oled);
    snake.begin();

    while(true){
        snake.tick(direction, millis());

        digitalWrite(HEARTBEAT_PIN, millis()%HEARTBEAT_PERIOD>HEARTBEAT_PERIOD/2);
        //delay(1);
    }

}