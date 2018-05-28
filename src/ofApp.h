#pragma once

#include "ofxAndroid.h"
#include "ofxBox2d.h"
#include "FluxlyClasses.h"
#include "PdExternals.h"
#include "ofxPd.h"
#include "ofxXmlSettings.h"
#include "ofxAndroidSoundStream.h"

#define FLUXLY_MAJOR_VERSION (1)
#define FLUXLY_MINOR_VERSION (0)

//  Determine device
//#define IS_IPAD (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
//#define IS_IPHONE (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
//#define IS_IPHONE_5 (IS_IPHONE && [[UIScreen mainScreen] bounds].size.height == 568)
//#define IS_IPHONE_6 (IS_IPHONE && [[UIScreen mainScreen] bounds].size.height == 667)
//#define IS_IPHONE_6_PLUS (IS_IPHONE && [[UIScreen mainScreen] bounds].size.height == 736)
//#define IS_IPHONE_X (IS_IPHONE && [[UIScreen mainScreen] bounds].size.height == 812)

// Defines for translating iPhone positioning to iPad
// 480 point wide -> 1024 points wide
// = 2.13 multiplier
//#define IPAD_BOT_TRIM (45)

// States
#define PAUSE (0)
#define RUN (1)
#define CHOOSE_LOOP (2)

// Scenes
#define MENU_SCENE (0)
#define GAME_SCENE (1)
#define RECORDING_SCENE (2)
#define SAVE_EXIT (3)
#define SELECT_SAMPLE_SCENE (4)

#define PHONE (0)
#define TABLET (1)

#define ONE_SECOND (60)
#define TWO_SECONDS (120)
#define THREE_SECONDS (180)
#define FOUR_SECONDS (240)

#define maxSamples (144)

#define MAIN_MENU (0)
#define SAMPLE_MENU (1)

#define maxTouches (11)
#define nScenes (16)

#define SAMPLES_IN_BUNDLE (15)

// a namespace for the Pd types
using namespace pd;

class BoxData {
public:
    int boxId;
};

class FluxlyConnection {
public:
    int id1;
    int id2;
};

class FluxlyJointConnection {
public:
    int id1;
    int id2;
    ofxBox2dJoint *joint;
};

class ofApp : public ofxAndroidApp, public PdReceiver, public PdMidiReceiver {

public:
    void setup();
    void update();
    void draw();
    void exit();

    void loadMenu();
    void loadGame(int gameId);
    void loadGameSettings();
    void saveGame();

    void touchDown(ofTouchEventArgs & touch);
    void touchMoved(ofTouchEventArgs & touch);
    void touchUp(ofTouchEventArgs & touch);
    void touchDoubleTap(ofTouchEventArgs & touch);
    void touchCancelled(ofTouchEventArgs & touch);
    void lostFocus();
    void gotFocus();
    void gotMemoryWarning();
    void deviceOrientationChanged(int newOrientation);

    void reloadSamples();
    void takeScreenshot();
    void contactStart(ofxBox2dContactArgs &e);
    void contactEnd(ofxBox2dContactArgs &e);

    void destroyGame();
    void helpLayerScript();
    void helpLayerDisplay(int n);
    void drawHelpString(string s, int x1, int y1, int yOffset, int row);

    void initAudio();


    bool notConnectedYet(int n1, int n2);
    bool complementaryColors(int n1, int n2);
    bool bothTouched(int n1, int n2);
    bool drawForScreenshot = false;
    bool doubleTapped = false;

    string errorMsg;
    string appDirectory;

    int inputDeviceID, outputDeviceID;
    int numInputChannels, numOutputChannels;
    int sampleRate, ticksPerBuffer;
    bool shouldReinitAudio;

    ofxPd pd;
    ofxXmlSettings globalSettings;
    ofxXmlSettings sampleList;

    //ofxAndroidSoundStream stream;
    ofSoundStream stream;

    vector<Patch> instances;

    // audio callbacks
    void audioReceived(float * input, int bufferSize, int nChannels);
    void audioRequested(float * output, int bufferSize, int nChannels);

    // sets the preferred sample rate, returns the *actual* samplerate
    // which may be different ie. iPhone 6S only wants 48k
    //float setAVSessionSampleRate(float preferredSampleRate);

    int startTouchId = 0;
    int startTouchX = 0;
    int startTouchY = 0;
    int helpTimer = 0;
    int currentHelpState = 0;

    int device = PHONE;
    float deviceScale = 1.0;
    int nIcons = 6;
    int nCircles;
    int scene = MENU_SCENE;
    int gameState = 0;
    bool firstRun = true;
    bool helpOn = true;

    int selected = -1;
    int midiChan;
    int tempo = 1;
    int X_OFFSET;
    int Y_OFFSET;
    int HITSPOT_X;
    int HITSPOT_Y;
    int HITSPOT_W;
    float SCALING;
    int IPAD_MARGIN;

    float backgroundX = 0;
    float prevBackgroundX = 0;
    float backgroundY = 0;
    int menuScreen = 0;
    int menuOrigin = 0;

    //The iPhone supports 5 simultaneous touches, and cancels them all on the 6th touch.
    //Current iPad models (through Air 2) support 11 simultaneous touches, and do nothing on a 12th
    //iPad Pro has 17?

    int touchX[maxTouches] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int touchY[maxTouches] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int touchControl[maxTouches] = {-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

    ofDirectory dir;
    ofFile file;
    int numFiles;
    int currentGame = -1;   // index in menu

    int touchMargin = 2;

    ofTrueTypeFont helpFont;
    string eventString;

    int worldW = 320;
    int worldH = 568;
    int screenW = 320;
    int screenH = 568;
    int toolbarH = 30;
    int consoleH = 200;
    int helpTextHeight;

    int iconW = 65;
    int menuItemsPerRow = 3;

    int globalTick = 0;
    int tempId1;
    int tempId2;
    int maxJoints = 3;
    int backgroundId = 0;
    bool applyDamping = true;

    ofImage background[8];
    ofImage foreground;
    ofImage pauseMotion;
    ofImage screenshot;
    ofImage exitButton;
    ofImage dampOnOff;
    ofImage dampOnOffGlow;
    ofImage saving;
    ofImage helpButton;
    ofImage helpButtonGlow;
    ofImage arrow;
    ofImage arrowLeft;

    ofxBox2d box2d;

    SlidingMenu * mainMenu;
    SlidingMenu * sampleMenu;
    SampleConsole * playRecordConsole;

    vector <shared_ptr<FluxlyCircle> > circles;
    vector <shared_ptr<FluxlyJointConnection> > joints;
    vector <shared_ptr<FluxlyConnection> > connections;
    vector <shared_ptr<FluxlyIcon> > icons;

    ofImage toolbar;

    ofRectangle bounds;


    /////////////////
    float 	pan;
    bool 	bNoise;
    float 	volume;

    float 	* lAudio;
    float   * rAudio;

    //------------------- for the simple sine wave synthesis
    float 	targetFrequency;
    float 	phase;
    float 	phaseAdder;
    float 	phaseAdderTarget;
};


