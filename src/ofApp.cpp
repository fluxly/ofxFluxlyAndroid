#include "ofApp.h"
#include "ofxAndroidUtils.h"

//--------------------------------------------------------------
void ofApp::setup(){

    ofSetLogLevel(OF_LOG_VERBOSE);       // OF_LOG_VERBOSE for testing
    ofSetLogLevel("Pd", OF_LOG_VERBOSE); // see verbose info inside

    /*if (IS_IPAD) {
        device = TABLET;
    }*/

    //ofLog(OF_LOG_VERBOSE, ofxiOSGetDocumentsDirectory());  // useful for accessing documents directory in simulator

    appDirectory = "";
    // On first run, check if settings files are in documents directory; if not, copy from the bundle
    dir.open(appDirectory);
    int numFiles = dir.listDir();
    firstRun = true;

    for (int i=0; i<numFiles; ++i) {
        if (dir.getName(i) == "menuSettings.xml") {
            firstRun = false;
            helpOn = false;        // turn off help layer if not first run
        }
        //cout << "Path at index " << i << " = " << dir.getName(i) << endl;
    }
    if (firstRun) {
        // ofLog(OF_LOG_VERBOSE, "First Run: copies files to documents from bundle.");
        file.copyFromTo("menuSettings.xml", appDirectory+"menuSettings.xml", true, true);
        file.copyFromTo("sampleSettings.xml", appDirectory+"sampleSettings.xml", true, true);
        for (int i=0; i < 15; i++) {
            if (device == TABLET) {
                file.copyFromTo("game"+to_string(i)+"-ipad.xml", appDirectory+"game"+to_string(i)+".xml", true, true);
                file.copyFromTo("game"+to_string(i)+"-ipad.png", appDirectory+"game"+to_string(i)+".png", true, true);
            } else {
                file.copyFromTo("game"+to_string(i)+".xml", appDirectory+"game"+to_string(i)+".xml", true, true);
                file.copyFromTo("game"+to_string(i)+".png", appDirectory+"game"+to_string(i)+".png", true, true);
            }
        }
    }

    // Set screen height and width
    //screenH = [[UIScreen mainScreen] bounds].size.height;
    //screenW = [[UIScreen mainScreen] bounds].size.width;
    screenW = 768;
    screenH = 1024;
    worldW = screenW;
    worldH = screenH;
    ofLog(OF_LOG_VERBOSE, "W, H %d, %d:",screenW, screenH);

    // try to set the preferred iOS sample rate, but get the actual sample rate
    // being used by the AVSession since newer devices like the iPhone 6S only
    // want specific values (ie 48000 instead of 44100)
    float sampleRate = 44100;

    // the number if libpd ticks per buffer,
    // used to compute the audio buffer len: tpb * blocksize (always 64)
    int ticksPerBuffer = 8; // 8 * 64 = buffer len of 512

    // setup OF sound stream using the current *actual* samplerate
    //stream = new ofxAndroidSoundStream();
    //ofSoundStreamSetup(2, 1, this, 44100, ofxPd::blockSize()*ticksPerBuffer, 3);
    //ofSoundStreamStart();
    //ofxAndroidRequestPermission(OFX_ANDROID_PERMISSION_RECORD_AUDIO);

    ofSoundStreamSettings settings;
    settings.numOutputChannels = 2;
    settings.numInputChannels = 1;
    settings.setOutListener(this);
    settings.setInListener(this);
    settings.numBuffers = 3;
    settings.sampleRate = sampleRate;
    settings.bufferSize = ofxPd::blockSize()*ticksPerBuffer;
    stream.setup(settings);
    stream.setOutput(this);
    stream.setInput(this);
	//stream.setup(this, 2, 1, sampleRate, ofxPd::blockSize()*ticksPerBuffer, 3);
    //stream.start();

    // setup Pd
    //
    // set 4th arg to true for queued message passing using an internal ringbuffer,
    // this is useful if you need to control where and when the message callbacks
    // happen (ie. within a GUI thread)
    //
    // note: you won't see any message prints until update() is called since
    // the queued messages are processed there, this is normal
    //
    if(!pd.init(2, 1, sampleRate, ticksPerBuffer-1, false)) {
        OF_EXIT_APP(1);
    }

    // Setup externals
    freeverb_tilde_setup();

    midiChan = 1; // midi channels are 1-16

    // subscribe to receive source names
    pd.subscribe("toOF");
    pd.subscribe("env");

    // add message receiver, required if you want to receieve messages
    pd.addReceiver(*this);   // automatically receives from all subscribed sources
    pd.ignoreSource(*this, "env");      // don't receive from "env"
    //pd.ignoreSource(*this);           // ignore all sources
    //pd.receiveSource(*this, "toOF");  // receive only from "toOF"

    // add midi receiver, required if you want to recieve midi messages
    pd.addMidiReceiver(*this);  // automatically receives from all channels
    //pd.ignoreMidiChannel(*this, 1);     // ignore midi channel 1
    //pd.ignoreMidiChannel(*this);        // ignore all channels
    //pd.receiveMidiChannel(*this, 1);    // receive only from channel 1

    // add the data/pd folder to the search path
    pd.addToSearchPath("pd/abs");

    ofSeedRandom();
    // audio processing on
    pd.start();
    pd.openPatch("YakShaveriOS3.pd");

    // Load all images
    for (int i=0; i<8; i++) {
        background[i].load("background" + std::to_string(i) + ".png");
        background[i].getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);    // for clean pixel scaling
    }
    //toolbar.load("toolbar.png");
    exitButton.load("navMenuExit.png");
    exitButton.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    helpButton.load("helpButton.png");
    helpButton.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    helpButtonGlow.load("helpButtonGlow.png");
    helpButtonGlow.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    arrow.load("arrow.png");
    arrow.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    arrowLeft.load("arrowLeft.png");
    arrowLeft.getTexture().setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);

    ofSetFrameRate(60);
    ofEnableAntiAliasing();

    bounds.set(0, 0, worldW, worldH-toolbarH);
    box2d.init();
    box2d.setFPS(60);
    box2d.setGravity(0, 0);
    box2d.createBounds(bounds);
    box2d.enableEvents();
    ofAddListener(box2d.contactStartEvents, this, &ofApp::contactStart);
    ofAddListener(box2d.contactEndEvents, this, &ofApp::contactEnd);

    box2d.registerGrabbing();

    if (device == PHONE) {
        helpFont.load("slkscr.ttf", 12);
    } else {
        helpFont.load("slkscr.ttf", 16);
    }
    helpTextHeight = helpFont.getLineHeight() * 0.8;
    ofLog(OF_LOG_VERBOSE, "Text line height: %d", helpTextHeight);

    mainMenu = new SlidingMenu();

    if (device==TABLET) {
        mainMenu->menuTitleFilename = "fluxlyTitle-ipad.png";
        mainMenu->menuTitleW = screenW;
        mainMenu->menuTitleH = 574*((float)screenW/2048);
    }
    mainMenu->initMenu(MAIN_MENU, 0, 0, screenW, screenH);

    sampleMenu = new SlidingMenu();
    sampleMenu->initMenu(SAMPLE_MENU, 0, consoleH, screenW, screenH);

    playRecordConsole = new SampleConsole();
    playRecordConsole->init(screenW, consoleH);

    // Do some scaling for tablet version
    if (device == TABLET) {
        deviceScale = 1.5;
        playRecordConsole->thumbW *= 2;
    }

    dampOnOff.load("dampOnOff.png");
    dampOnOffGlow.load("dampOnOffGlow.png");

    saving.load("saving.png");
}

void ofApp::loadGame(int gameId) {
    //dir.open(ofxiOSGetDocumentsDirectory());
    //numFiles = dir.listDir();
    //for (int i=0; i<numFiles; ++i) {
    //    cout << "Path at index " << i << " = " << dir.getPath(i) << endl;
    //}

    // the world bounds
    currentGame = gameId;

    //send screen width for panning calculation in Pd
    pd.sendFloat("screenW", screenW);

    ofxXmlSettings gameSettings;
    if (gameSettings.loadFile(appDirectory+mainMenu->menuItems[gameId]->link)) {
        string menuItemName = gameSettings.getValue("settings:menuItem", "defaultScene");
        backgroundId = gameSettings.getValue("settings:settings:backgroundId", 0);

        gameSettings.pushTag("settings");
        gameSettings.pushTag("circles");
        nCircles = gameSettings.getNumTags("circle");

        for(int i = 0; i < nCircles; i++){
            //ofLog(OF_LOG_VERBOSE, "Circle count: %d", i);
            circles.push_back(shared_ptr<FluxlyCircle>(new FluxlyCircle));
            FluxlyCircle * c = circles.back().get();
            gameSettings.pushTag("circle", i);
            c->id = gameSettings.getValue("id", 0);
            c->type =  gameSettings.getValue("type", 0);
            c->eyeState =  gameSettings.getValue("eyestate", true);
            c->onOffState = gameSettings.getValue("onOffState", true);
            c->spinning =  gameSettings.getValue("spinning", true);
            c->wasntSpinning =  gameSettings.getValue("wasntspinning", true);
            c->instrument =  gameSettings.getValue("instrument", 0);
            c->dampingX =  gameSettings.getValue("dampingX", 0);
            c->dampingY =  gameSettings.getValue("dampingY", 0);
            c->connections[0] =  gameSettings.getValue("connection1", 0);
            c->connections[1] =  gameSettings.getValue("connection2", 0);
            c->connections[2] =  gameSettings.getValue("connection3", 0);
            c->connections[3] =  gameSettings.getValue("connection4", 0);
            c->x = gameSettings.getValue("x", 0);
            c->y = gameSettings.getValue("y", 0);
            c->w = gameSettings.getValue("w", 0);
            c->displayW = c->w;

            // Make some corrections for tablets
            if (device == TABLET) {
                c->soundWaveStep = 4;
                c->soundWaveH = 100;
                c->soundWaveStart = -1024;
                c->maxAnimationCount = 100;
                c->animationStep = 12;
                c->displayW *= deviceScale;
            }

            c->setPhysics(1, 1, 1);    // density, bounce, friction
            c->setup(box2d.getWorld(), c->x, c->y, (c->w/2)*deviceScale);
            c->setRotation(gameSettings.getValue("rotation", 0));
            BoxData * bd = new BoxData();
            bd->boxId = c->id;
            c->body->SetUserData(bd);
            c->init();
            if ((c->type < SAMPLES_IN_BUNDLE)) {
                // The built-in samples are in the bundle
                pd.sendSymbol("filename"+to_string(circles[i].get()->instrument), sampleMenu->menuItems[circles[i].get()->type]->link);
            } else {
                if (c->type < 144) {
                    // Anything after that is in the documents directory
                    pd.sendSymbol("filename"+to_string(circles[i].get()->instrument),
                                  appDirectory+sampleMenu->menuItems[circles[i].get()->type]->link);
                }
            }
            pd.sendFloat("tempo8", 0.0);    // Set reverb to 0
            gameSettings.popTag();
        }
        gameSettings.popTag();
        gameSettings.pushTag("joints");
        int nJoints = gameSettings.getNumTags("joint");
        //ofLog(OF_LOG_VERBOSE, "nJoints: %d", nJoints);
        for(int i = 0; i < nJoints; i++){
            gameSettings.pushTag("joint", i);
            int id1 = gameSettings.getValue("id1", 0);
            int id2 = gameSettings.getValue("id2", 0);
            //ofLog(OF_LOG_VERBOSE, "Joint: id1, id2: %d, %d", id1, id2);
            gameSettings.popTag();
        }
    } else {
        ofLog(OF_LOG_VERBOSE, "Couldn't load file!");
    }
    applyDamping = true;

    pd.sendFloat("masterVolume", 1.0);
    gameState = RUN;
}

static bool shouldRemoveConnection(shared_ptr<FluxlyConnection>shape) {
    return true;
}
static bool shouldRemoveJoint(shared_ptr<FluxlyJointConnection>shape) {
    return true;
}
static bool shouldRemoveCircle(shared_ptr<FluxlyCircle>shape) {
    return true;
}

//--------------------------------------------------------------
void ofApp::update() {
    switch (scene) {
        case MENU_SCENE:
            mainMenu->updateScrolling();

            break;
        case GAME_SCENE:
            if (gameState == RUN) {
                // since this is a test and we don't know if init() was called with
                // queued = true or not, we check it here
                if(pd.isQueued()) {
                    ofLog(OF_LOG_VERBOSE, "Queued!");
                    // process any received messages, if you're using the queue and *do not*
                    // call these, you won't receieve any messages or midi!
                    pd.receiveMessages();
                    pd.receiveMidi();
                }

                box2d.update();

                globalTick++;

                // only update one pan channel each tick
                int panChannel = globalTick % nCircles;
                pd.sendFloat("pan"+to_string(panChannel), circles[panChannel]->x);

                for (int i=0; i<circles.size(); i++) {
                    if ((circles[i]->spinning) && (circles[i]->wasntSpinning)) {
                        circles[i]->onOffState = true;
                        circles[i]->wasntSpinning = false;
                    }
                    if (circles[i]->spinning) {
                        if (circles[i]->onOffState) {
                            circles[i]->eyeState = true;
                        } else {
                            circles[i]->eyeState = false;
                        }
                    } else {
                        circles[i]->onOffState = false;
                        circles[i]->eyeState = false;
                        circles[i]->wasntSpinning = true;
                    }
                }

                for (int i=0; i < circles.size(); i++) {
                    circles[i].get()->setRotationFriction(1);
                    if (applyDamping) circles[i].get()->setDamping(0, 0);
                    circles[i].get()->checkToSendNote();
                    circles[i].get()->checkToSendTempo();

                    if (circles[i].get()->sendTempo) {
                        ofLog(OF_LOG_VERBOSE, "Changed tempo %d: %f", i, circles[i]->tempo);
                        pd.sendFloat("tempo"+to_string(circles[i].get()->instrument), circles[i]->tempo);
                        circles[i].get()->sendTempo = false;
                    }
                    if (circles[i].get()->sendOn) {
                        pd.sendFloat("toggle"+to_string(circles[i].get()->instrument), 1.0);
                        circles[i].get()->sendOn = false;
                    }
                    if (circles[i].get()->sendOff) {
                        pd.sendFloat("toggle"+to_string(circles[i].get()->instrument), 0.0);
                        circles[i].get()->sendOff = false;
                    }
                    if (circles[i].get()->type < 144){
                        pd.readArray("scope"+to_string(circles[i].get()->instrument), circles[i].get()->scopeArray);
                        //ofLog(OF_LOG_VERBOSE, "Get scope %i:", circles[i].get()->instrument);
                    }
                }

                if (connections.size() > 0) {
                    // if (false) {
                    for (int i=0; i<connections.size(); i++) {
                        //ofLog(OF_LOG_VERBOSE, "List size: %d  id1: %d  id2: %d", connections.size(), connections[i]->id1, connections[i]->id2);

                        ofLog(OF_LOG_VERBOSE, "TEST CONNECT: %d -> %d", connections[i]->id1, connections[i]->id2);

                        tempId1 = connections[i]->id1;
                        tempId2 = connections[i]->id2;
                        if ((tempId1 <144) && (tempId1 >=0) && (tempId2 <144) && (tempId2 >=0)) {
                            // Add joints
                            if ((circles[tempId1]->nJoints < maxJoints) && (circles[tempId2]->nJoints < maxJoints)
                                && notConnectedYet(tempId1, tempId2) && bothTouched(tempId1, tempId2)) { //&& complementaryColors(tempId1, tempId2)) {

                                //  ofLog(OF_LOG_VERBOSE, "CONNECT: %d -> %d", tempId1, tempId2);

                                shared_ptr<FluxlyJointConnection> jc = shared_ptr<FluxlyJointConnection>(new FluxlyJointConnection);
                                ofxBox2dJoint *j = new ofxBox2dJoint;
                                j->setup(box2d.getWorld(), circles[tempId1].get()->body, circles[tempId2].get()->body);
                                j->setLength(circles[tempId1]->w/2 + circles[tempId2]->w/2 - 2);
                                jc.get()->id1 = tempId1;
                                jc.get()->id2 = tempId2;
                                jc.get()->joint = j;
                                joints.push_back(jc);
                                circles[tempId1]->nJoints++;
                                circles[tempId2]->nJoints++;
                            }
                        }
                    }
                }
                // Remove everything from connections vector
                ofRemove(connections, shouldRemoveConnection);
                //ofLog(OF_LOG_VERBOSE, "Connections after remove: %d", connections.size());
            }
            break;
        case SELECT_SAMPLE_SCENE:
            sampleMenu->updateScrolling();
            if (!playRecordConsole->playing && !playRecordConsole->recording) pd.readArray("previewScope", playRecordConsole->scopeArray);
            if (playRecordConsole->playing) pd.readArray("previewScope", playRecordConsole->scopeArray);
            if (playRecordConsole->recording) pd.readArray("inputScope", playRecordConsole->scopeArray);
            break;
    }
    if (helpOn) {
        helpLayerScript();     // update the help layer
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
    switch (scene) {
        case MENU_SCENE:
            mainMenu->draw();
            mainMenu->drawBorder(currentGame);
            break;
        case GAME_SCENE:
            ofSetHexColor(0xFFFFFF);
            ofSetRectMode(OF_RECTMODE_CORNER);
            background[backgroundId].draw(0, 0, worldW, worldH);
            ofSetRectMode(OF_RECTMODE_CENTER);

            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->drawAnimation(1);
            }
            //if (device == PHONE) {

            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->drawSoundWave(1);
            }
            // }
            ofSetRectMode(OF_RECTMODE_CENTER);
            ofSetHexColor(0xFFFFFF);

            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->draw();
            }

            for (int i=0; i<joints.size(); i++) {
                ofSetColor( ofColor::fromHex(0xff0000) );
                joints[i]->joint->draw();
            }
            ofSetHexColor(0xFFFFFF);
            exitButton.draw(screenW-18, screenH-18, 32, 32);
            if (applyDamping) {
                dampOnOff.draw(18, screenH-18, 32, 32);
            } else {
                dampOnOffGlow.draw(18, screenH-18, 32, 32);
            }
            if (!helpOn) {
                helpButton.draw(screenW/2, screenH-18, 32, 32);
            } else {
                helpButtonGlow.draw(screenW/2, screenH-18, 32, 32);
            }
            //helpFont.drawString(ofToString(ofGetFrameRate()), 10,20);
            break;
        case SELECT_SAMPLE_SCENE:
            ofSetHexColor(0xFFFFFF);
            ofSetRectMode(OF_RECTMODE_CORNER);
            background[backgroundId].draw(0, 0, worldW, worldH);
            ofSetRectMode(OF_RECTMODE_CENTER);
            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->draw();
            }

            ofSetColor(25, 25, 25, 200);
            ofDrawRectangle(screenW/2, screenH/2, screenW, screenH);
            ofSetRectMode(OF_RECTMODE_CENTER);
            ofSetColor(255, 255, 255, 255);
            sampleMenu->draw();
            playRecordConsole->draw();
            ofSetColor(255, 255, 255, 200);
            ofDrawRectangle(18, 18, 30, 30);
            ofSetColor(255, 255, 255, 255);
            exitButton.draw(18, 18, 32, 32);
            break;
        case SAVE_EXIT:
            ofSetHexColor(0xFFFFFF);
            ofSetRectMode(OF_RECTMODE_CORNER);
            background[backgroundId].draw(0, 0, worldW, worldH);
            ofSetRectMode(OF_RECTMODE_CENTER);
            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->drawSoundWave(3);
            }
            for (int i=0; i<circles.size(); i++) {
                circles[i].get()->draw();
            }
            for (int i=0; i<joints.size(); i++) {
                ofSetColor( ofColor::fromHex(0xff0000) );
                joints[i]->joint->draw();
            }
            screenshot.grabScreen(0, 0, screenW, screenH);
            screenshot.save( mainMenu->menuItems[currentGame]->filename);
            saving.draw(screenW/2, screenH/2);
            //ofLog(OF_LOG_VERBOSE, "Screenshot");
            saveGame();
            destroyGame();
            scene = MENU_SCENE;
            mainMenu->menuItems[currentGame]->reloadThumbnail();
            break;
    }
    if (helpOn) {
        helpLayerDisplay(currentHelpState);
    }
}

//--------------------------------------------------------------
void ofApp::exit(){
    pd.sendFloat("masterVolume", 0.0);
}

void ofApp::saveGame() {
    ofxXmlSettings outputSettings;

    outputSettings.addTag("settings");
    outputSettings.pushTag("settings");
    outputSettings.setValue("settings:fluxlyMajorVersion", FLUXLY_MAJOR_VERSION);
    outputSettings.setValue("settings:fluxlyMinorVersion", FLUXLY_MINOR_VERSION);
    outputSettings.setValue("settings:backgroundId", backgroundId);
    outputSettings.addTag("circles");
    outputSettings.pushTag("circles");
    for (int i = 0; i < nCircles; i++){
        outputSettings.addTag("circle");
        outputSettings.pushTag("circle", i);
        outputSettings.setValue("id", circles[i]->id);
        outputSettings.setValue("type", circles[i]->type);
        outputSettings.setValue("eyeState", false);
        outputSettings.setValue("onOffState", false);
        outputSettings.setValue("spinning", false);
        outputSettings.setValue("wasntSpinning", false);
        outputSettings.setValue("dampingX", circles[i]->dampingX);
        outputSettings.setValue("dampingY", circles[i]->dampingY);
        outputSettings.setValue("instrument", circles[i]->instrument);
        outputSettings.setValue("connection1", circles[i]->connections[0]);
        outputSettings.setValue("connection2", circles[i]->connections[1]);
        outputSettings.setValue("connection3", circles[i]->connections[2]);
        outputSettings.setValue("connection4", circles[i]->connections[3]);
        outputSettings.setValue("x", circles[i]->x);
        outputSettings.setValue("y", circles[i]->y);
        outputSettings.setValue("w", circles[i]->w);
        outputSettings.setValue("rotation", circles[i]->rotation);
        outputSettings.popTag();
    }
    outputSettings.popTag();
    outputSettings.addTag("joints");
    outputSettings.pushTag("joints");
    for(int i = 0; i < joints.size(); i++){
        outputSettings.addTag("joint");
        outputSettings.pushTag("joint", i);
        outputSettings.setValue("id1", 0);
        outputSettings.setValue("id2", 0);
        outputSettings.popTag();
    }
    outputSettings.popTag();
    outputSettings.popTag();
    outputSettings.saveFile(appDirectory+"game"+to_string(currentGame)+".xml");

}

void ofApp::destroyGame() {

    pd.sendFloat("masterVolume", 0.0);

    for (int i=0; i < circles.size(); i++) {
        pd.sendFloat("toggle"+to_string(circles[i].get()->instrument), 0.0);
    }

    for (int i=0; i < joints.size(); i++) {
        // remove joint from world
        joints[i]->joint->destroy();
    }

    for (int i=0; i < circles.size(); i++) {
        delete (BoxData *)circles[i]->body->GetUserData();
        circles[i]->destroy();
    }

    ofRemove(joints, shouldRemoveJoint);
    ofRemove(circles, shouldRemoveCircle);
}


void ofApp::reloadSamples() {
    for (int i=0; i<circles.size(); i++) {
        if ((circles[i]->type < SAMPLES_IN_BUNDLE)) {
            pd.sendSymbol("filename"+to_string(circles[i].get()->instrument), sampleMenu->menuItems[circles[i].get()->type]->link);
        } else {
            if (circles[i]->type < 144) {
                // Anything after that is in the documents directory
                pd.sendSymbol("filename"+to_string(circles[i].get()->instrument),
                              appDirectory+sampleMenu->menuItems[circles[i].get()->type]->link);
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::touchDown(ofTouchEventArgs & touch){
    ofLog(OF_LOG_VERBOSE, "TOUCH DOWN!");
    // MENU SCENE: Touched and not already moving: save touch down location and id
    if ((scene == MENU_SCENE) && (mainMenu->scrollingState == 0)) {
        mainMenu->scrollingState = -1;  // wait for move state
        //ofLog(OF_LOG_VERBOSE, "Scrolling State %d", mainMenu->scrollingState);
        //startBackgroundY = backgroundY;
        startTouchId = touch.id;
        startTouchX = (int)touch.x;
        startTouchY = (int)touch.y;
    }

    // SELECT SAMPLE SCENE: Touched and not already moving: save touch down location and id
    if ((scene == SELECT_SAMPLE_SCENE) && (sampleMenu->scrollingState == 0)) {
        sampleMenu->scrollingState = -1;  // wait for move state
        ofLog(OF_LOG_VERBOSE, "Scrolling State %d", sampleMenu->scrollingState);
        //startBackgroundY = backgroundY;
        startTouchId = touch.id;
        startTouchX = (int)touch.x;
        startTouchY = (int)touch.y;
    }

    if (scene == GAME_SCENE) {
        startTouchId = touch.id;
        startTouchX = (int)touch.x;
        startTouchY = (int)touch.y;

        for (int i=0; i<circles.size(); i++) {
            if (circles[i]->inBounds(touch.x, touch.y) && !circles[i]->touched) {
                circles[i]->touched = true;
                circles[i]->touchId = touch.id;
                //ofLog(OF_LOG_VERBOSE, "Touched %d", i);
            }
        }
    }
    /*
    if (scene == SELECT_SAMPLE_SCENE) {
        startTouchId = touch.id;
        startTouchX = (int)touch.x;
        startTouchY = (int)touch.y;
    }
    */
}


//--------------------------------------------------------------
void ofApp::touchMoved(ofTouchEventArgs & touch){
    // ofLog(OF_LOG_VERBOSE, "touch %d move at (%i,%i)", touch.id, (int)touch.x, (int)touch.y);

    // MENU SCENE: no longer in same place as touch down
    // added a bit to the bounds to account for higher res digitizers
    if (scene == MENU_SCENE) {
        if ((mainMenu->scrollingState == -1) && (startTouchId == touch.id)) {

            if ((touch.y < (startTouchY - touchMargin*2)) || (touch.y > (startTouchY + touchMargin*2))) {
                mainMenu->scrollingState = 1;
            }
        }

        // MENU SCENE: Moving with finger down: slide menu up and down
        if ((mainMenu->scrollingState == 1)  && (startTouchId == touch.id)) {
            mainMenu->menuY = mainMenu->menuOriginY + ((int)touch.y - startTouchY);
        }
    }

    // SELECT SAMPLE SCENE
    if (scene == SELECT_SAMPLE_SCENE) {
        if ((sampleMenu->scrollingState == -1) && (startTouchId == touch.id)) {
            if ((touch.y < (startTouchY - touchMargin*2)) || (touch.y > (startTouchY + touchMargin*2))) {
                sampleMenu->scrollingState = 1;
            }
        }
        //ofLog(OF_LOG_VERBOSE, "menuY before, touch.y, startTouchY %f, %f, %f, %i", sampleMenu->menuY, sampleMenu->menuOriginY, touch.y, startTouchY);
        // SELECT SAMPLE SCENE: Moving with finger down: slide menu up and down
        if ((sampleMenu->scrollingState == 1)  && (startTouchId == touch.id)) {
            sampleMenu->menuY = sampleMenu->menuOriginY + (touch.y - startTouchY);
        }
        ofLog(OF_LOG_VERBOSE, "menuY after %f", sampleMenu->menuY);
    }
}

//--------------------------------------------------------------
void ofApp::touchUp(ofTouchEventArgs & touch){
    ofLog(OF_LOG_VERBOSE, "touch %d up at (%i,%i)", touch.id, (int)touch.x, (int)touch.y);
    if ((scene == SELECT_SAMPLE_SCENE) && (doubleTapped)) {
        doubleTapped = false;
        sampleMenu->scrollingState = 0;
        startTouchId = -1;
        startTouchX = 0;
        startTouchY = 0;
        ofLog(OF_LOG_VERBOSE, "----> %f, %f", touch.x, touch.y);
    } else {
        // MENU SCENE: Touched but not moved: load instrument
        if ((scene == MENU_SCENE) && (mainMenu->scrollingState == -1) && (startTouchId == touch.id)) {
            //ofLog(OF_LOG_VERBOSE, "scrollingState? %i", mainMenu->scrollingState);
            mainMenu->scrollingState = 0;
            startTouchId = -1;
            startTouchX = 0;
            startTouchY = 0;
            //ofLog(OF_LOG_VERBOSE, "checking %f, %f", touch.x, touch.y);
            int selectedGame = mainMenu->checkMenuTouch(touch.x, touch.y);
            if ( selectedGame > -1) {
                loadGame(selectedGame);
                scene = GAME_SCENE;
            }
        }
        // MENU SCENE: Touch up after moving
        if ((scene == MENU_SCENE) && (mainMenu->scrollingState == 1) && (startTouchId == touch.id)) {
            // If moved sufficiently, switch to next or previous state
            if ((int)touch.y < startTouchY-75) {
                mainMenu->changePaneState(-1);
            } else {
                if ((int)touch.y > startTouchY+75) {
                    mainMenu->changePaneState(1);
                }
            }
            mainMenu->scrollingState = 2;
            /*menuOrigin = -screenH*scrollingState;
             
            startBackgroundY = backgroundY;*/
            mainMenu->menuMoveStep = abs(mainMenu->menuY - mainMenu->menuOriginY)/8;
            startTouchId = -1;
            startTouchX = 0;
            startTouchY = 0;
            //ofLog(OF_LOG_VERBOSE, "New State: %i", mainMenu->scrollingState);
        }

        // SELECT SAMPLE SCENE: Touched but not moved
        if ((scene == SELECT_SAMPLE_SCENE) && (sampleMenu->scrollingState == -1) && (startTouchId == touch.id)) {
            ofLog(OF_LOG_VERBOSE, "scrollingState? %i", sampleMenu->scrollingState);
            sampleMenu->scrollingState = 0;
            startTouchId = -1;
            startTouchX = 0;
            startTouchY = 0;
            ofLog(OF_LOG_VERBOSE, "checking %f, %f", touch.x, touch.y);
            // Check if touched
            int selectedSample = sampleMenu->checkMenuTouch(touch.x, touch.y);
            if (selectedSample > -1)  {
                //ofLog(OF_LOG_VERBOSE, "Selected: %i", selectedSample);
                sampleMenu->selected = selectedSample;
                playRecordConsole->setSelected(sampleMenu->menuItems[selectedSample]->id);
                sampleMenu->updateEyeState();
                if ((sampleMenu->menuItems[selectedSample]->id < SAMPLES_IN_BUNDLE)) {
                    //ofLog(OF_LOG_VERBOSE, "Load preview buffer: %i", selectedSample);
                    //ofLog(OF_LOG_VERBOSE, sampleMenu->menuItems[selectedSample]->link);
                    pd.sendSymbol("previewFilename", sampleMenu->menuItems[selectedSample]->link);
                } else {
                    if (sampleMenu->menuItems[selectedSample]->id < 144) {
                        // Anything after that is in the documents directory
                        pd.sendSymbol("previewFilename",
                                      appDirectory+sampleMenu->menuItems[selectedSample]->link);
                    }
                }
            }
        }

        // SELECT SAMPLE SCENE: Touch up after moving
        if ((scene == SELECT_SAMPLE_SCENE) && (sampleMenu->scrollingState == 1) && (startTouchId == touch.id)) {
            // If moved sufficiently, switch to next or previous state
            if ((int)touch.y < startTouchY-75) {
                sampleMenu->changePaneState(-1);
            } else {
                if ((int)touch.y > startTouchY+75) {
                    sampleMenu->changePaneState(1);
                }
            }
            sampleMenu->scrollingState = 2;
            sampleMenu->menuMoveStep = abs(sampleMenu->menuY - sampleMenu->menuOriginY)/8;
            startTouchId = -1;
            startTouchX = 0;
            startTouchY = 0;
            ofLog(OF_LOG_VERBOSE, "New State: %i", sampleMenu->scrollingState);
        }

        // GAME SCENE
        if (scene == GAME_SCENE) {
            for (int i=0; i<circles.size(); i++) {
                if (circles[i]->touchId == touch.id) {
                    circles[i]->touched = false;
                    circles[i]->touchId = -1;
                }
            }
            //ofLog(OF_LOG_VERBOSE, "Checking exit: %i, %i, %f, %f", startTouchX, startTouchY, touch.x, touch.y);
            // Check to see if exit pushed
            if ((startTouchX > screenW-36) && (startTouchY > (screenH- 36)) &&
                (touch.x > screenW-36) && (touch.y > (screenH- 36))) {
                scene = SAVE_EXIT;
                startTouchId = -1;
                startTouchX = 0;
                startTouchY = 0;
                //ofLog(OF_LOG_VERBOSE, "EXIT SCENE: %i", scene);
            }
            // Check to see if dampOnOff pushed
            if ((startTouchX < 36) && (startTouchY > (screenH- 36)) &&
                (touch.x < 36) && (touch.y > (screenH- 36))) {
                applyDamping = !applyDamping;
                startTouchId = -1;
                startTouchX = 0;
                startTouchY = 0;
                //ofLog(OF_LOG_VERBOSE, "DAMP ON OFF ");
            }
            // Check to see if helpOn pushed
            if ((startTouchX < (screenW/2+36)) && (startTouchX > (screenW/2-36)) &&
                (startTouchY > (screenH-36)) && (touch.x < (screenW/2+36)) && (touch.y > (screenW/2-36)) &&
                (touch.y > (screenH-36))) {
                helpOn = !helpOn;
                startTouchId = -1;
                startTouchX = 0;
                startTouchY = 0;
                //ofLog(OF_LOG_VERBOSE, "DAMP ON OFF ");
            }
        }

        // SAMPLE_SELECT_SCENE: Check all buttons
        if (scene == SELECT_SAMPLE_SCENE) {
            //ofLog(OF_LOG_VERBOSE, "Checking exit: %i, %i, %f, %f", startTouchX, startTouchY, touch.x, touch.y);
            // Check to see if exit pushed
            if ((startTouchX < 36) && (startTouchY < 36) &&
                (touch.x < 36) && (touch.y < 36)) {
                scene = GAME_SCENE;
                pd.sendFloat("masterVolume", 1.0);
                startTouchId = -1;
                startTouchX = 0;
                startTouchY = 0;
                ofLog(OF_LOG_VERBOSE, "EXIT SCENE: %i", scene);
                circles[sampleMenu->circleToChange]->type = sampleMenu->selected;
                circles[sampleMenu->circleToChange]->setMesh();
                reloadSamples();
                playRecordConsole->playing = false;
                playRecordConsole->recording = false;
                pd.sendFloat("togglePreview", 0.0);
                //ofLog(OF_LOG_VERBOSE, "Changing: %i, %i", sampleMenu->circleToChange,sampleMenu->selected);
            }
            int button = playRecordConsole->checkConsoleButtons(touch.x, touch.y);
            if (button == 1) {  //Play button
                ofLog(OF_LOG_VERBOSE, "Yup, play pressed");
                if (playRecordConsole->playing) {
                    pd.sendFloat("previewTempo", 1.0);
                    pd.sendFloat("togglePreview", 1.0);
                } else {
                    pd.sendFloat("previewTempo", 1.0);
                    pd.sendFloat("togglePreview", 0.0);
                }
            }
            if (button == 2) {  // Record button
                ofLog(OF_LOG_VERBOSE, "Yup, record pressed");
                if (playRecordConsole->selected >= SAMPLES_IN_BUNDLE) {
                    if (playRecordConsole->recording) {
                        pd.sendBang("startRecording");
                        ofLog(OF_LOG_VERBOSE, "Start recording");
                    } else {
                        ofLog(OF_LOG_VERBOSE, "Stop recording");
                        pd.sendBang("stopRecording");
                        pd.sendSymbol("writeRecordingToFilename", appDirectory+sampleMenu->menuItems[playRecordConsole->selected]->link);
                        pd.sendSymbol("previewFilename",
                                      appDirectory+sampleMenu->menuItems[playRecordConsole->selected]->link);
                        ofLog(OF_LOG_VERBOSE, appDirectory+sampleMenu->menuItems[playRecordConsole->selected]->link);
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::touchDoubleTap(ofTouchEventArgs & touch) {
    ofLog(OF_LOG_VERBOSE, "TOUCH DOUBLE TAP!");
    doubleTapped = true;

    if (scene == GAME_SCENE) {
        ofLog(OF_LOG_VERBOSE, "1. State %d", gameState);

        int retval = -1;
        for (int i=0; i<circles.size(); i++) {
            if (circles[i]->inBounds(touch.x, touch.y) && (circles[i]->type < 144)) {
                if (circles[i]->onOffState == false) retval = i;
                if (circles[i]->onOffState == true) circles[i]->onOffState = false;
            }
        }
        if (retval > -1) {
            scene = SELECT_SAMPLE_SCENE;
            pd.sendFloat("masterVolume", 0.0);
            playRecordConsole->playing = false;
            playRecordConsole->recording = false;
            sampleMenu->selected = circles[retval]->type;
            playRecordConsole->setSelected(circles[retval]->type);
            sampleMenu->circleToChange = retval;
            if ((sampleMenu->menuItems[retval]->id < SAMPLES_IN_BUNDLE)) {
                ofLog(OF_LOG_VERBOSE, "Load preview buffer: %i", retval);
                ofLog(OF_LOG_VERBOSE, sampleMenu->menuItems[circles[retval]->type]->link);
                pd.sendSymbol("previewFilename", sampleMenu->menuItems[circles[retval]->type]->link);
            } else {
                if (sampleMenu->menuItems[retval]->id < 144) {
                    // Anything after that is in the documents directory
                    pd.sendSymbol("previewFilename",
                                  appDirectory+sampleMenu->menuItems[circles[retval]->type]->link);
                }
            }
        } else {
            selected = -1;
        }

    }
}

//--------------------------------------------------------------
void ofApp::touchCancelled(ofTouchEventArgs & touch){

}

//--------------------------------------------------------------
void ofApp::lostFocus(){

}

//--------------------------------------------------------------
void ofApp::gotFocus(){

}

//--------------------------------------------------------------
void ofApp::gotMemoryWarning(){

}

//--------------------------------------------------------------
void ofApp::deviceOrientationChanged(int newOrientation){

}

void ofApp::contactStart(ofxBox2dContactArgs &e) {
    if(e.a != NULL && e.b != NULL) {
    }
}

//--------------------------------------------------------------
void ofApp::contactEnd(ofxBox2dContactArgs &e) {
    if(e.a != NULL && e.b != NULL) {
        b2Body *b1 = e.a->GetBody();
        BoxData *bd1 = (BoxData *)b1->GetUserData();
        if (bd1 !=NULL) {
            b2Body *b2 = e.b->GetBody();
            BoxData *bd2 = (BoxData *)b2->GetUserData();
            if (bd2 !=NULL) {
                // Add to list of connections to make in the update
                connections.push_back(shared_ptr<FluxlyConnection>(new FluxlyConnection));
                FluxlyConnection * c = connections.back().get();
                c->id1 = bd1->boxId;
                c->id2 = bd2->boxId;
            }
        }
    }
}

//--------------------------------------------------------------

bool ofApp::notConnectedYet(int n1, int n2) {
    bool retVal = true;
    int myId1;
    int myId2;
    for (int i=0; i < joints.size(); i++) {
        myId1 = joints[i]->id1;
        myId2 = joints[i]->id2;
        if (((n1 == myId1) && (n2 == myId2)) || ((n2 == myId1) && (n1 == myId2))) {
            //  ofLog(OF_LOG_VERBOSE, "Checking box %d connection list (length %d): %d == %d, %d == %d: Already connected",
            //  n1, boxen[n1]->nJoints, n1, myId1, n1, myId2);
            retVal = false;
        } else {
            // ofLog(OF_LOG_VERBOSE, "Checking box %d connection list (length %d): %d == %d, %d == %d: Not Yet connected",
            //      n1, boxen[n1]->nJoints, n1, myId1, n1, myId2);
        }
    }
    return retVal;
}

bool ofApp::complementaryColors(int n1, int n2) {
    bool retVal = false;
    if ((abs(circles[n1]->type - circles[n2]->type) == 1) || ((n1 == 0) || (n2 == 0))) {
        // ofLog(OF_LOG_VERBOSE, "    CORRECT COLOR");
        retVal = true;
    } else {
        // ofLog(OF_LOG_VERBOSE, "    WRONG COLOR");
    }
    return retVal;
}

bool ofApp::bothTouched(int n1, int n2) {
    bool retVal = false;
    if (circles[n1]->touched && circles[n2]->touched) {
        //ofLog(OF_LOG_VERBOSE, "    BOTH TOUCHED");
        retVal = true;
    } else {
        //ofLog(OF_LOG_VERBOSE, "    NOT BOTH TOUCHED");
    }
    return retVal;
}

//--------------------------------------------------------------
void ofApp::audioReceived(float * input, int bufferSize, int nChannels) {
    pd.audioIn(input, bufferSize, nChannels);
}

//--------------------------------------------------------------
void ofApp::audioRequested(float * output, int bufferSize, int nChannels) {
    pd.audioOut(output, bufferSize, nChannels);
}


void ofApp::helpLayerScript() {
    switch (scene) {
        case (MENU_SCENE) :
        case (SELECT_SAMPLE_SCENE) :
        case (SAVE_EXIT):
            //helpTimer = 0;
            //currentHelpState = -1;
            break;
        case (GAME_SCENE) :
            helpTimer = (helpTimer + 1) % (THREE_SECONDS * 19);
            currentHelpState = helpTimer / THREE_SECONDS;
            if (currentHelpState == 18) {
                helpTimer = 0;
                currentHelpState = -1;
                helpOn = false;
            }
            //ofLog(OF_LOG_VERBOSE, "timer %i", currentHelpState);
            break;
    }
}

void ofApp::helpLayerDisplay(int n) {
    if (scene == GAME_SCENE) {
        ofSetColor(0, 0, 0);
        int yOffset = circles[0]->w/2+helpTextHeight*(1+device*.8)*deviceScale;  // add space if tablet

        int x1;
        int y1;
        switch (n) {
            case -1:
                break;
            case 0:
                drawHelpString("These are fluxum", screenW/2, screenH/2-40, 0, 0);
                //helpFont.drawString("These are fluxum", screenW/2-helpFont.stringWidth("These are fluxum")/2, screenH/2);
                break;
            case 1:
                drawHelpString("These are fluxum", screenW/2, screenH/2-40, 0, 0);
                //helpFont.drawString("These are fluxum", screenW/2-helpFont.stringWidth("These are fluxum")/2, screenH/2);
                for (int i=0; i<circles.size()-1; i++) {
                    drawHelpString("fluxum", circles[i]->x, circles[i]->y, yOffset, 0);
                    /*helpFont.drawString("fluxum", circles[i]->x-helpFont.stringWidth("fluxum")/2, circles[i]->y+circles[i]->w/2+25*deviceScale);*/
                }
                break;
            case 2:
                drawHelpString("Fluxum are sound loopers", screenW/2, screenH/2-40, 0, 0);
                //helpFont.drawString("Fluxum are sound loopers", screenW/2-helpFont.stringWidth("Fluxum are sound loopers")/2, screenH/2);
                break;
            case 3:
            case 4:
            case 5:
                drawHelpString("Fluxum are sound loopers", screenW/2, screenH/2-40, 0, 0);
                for (int i=0; i<circles.size()-1; i++) {
                    drawHelpString("spin me", circles[i]->x, circles[i]->y, yOffset, 0);
                }
                break;
            case 6:
            case 7:
            case 8:
                for (int i=0; i<circles.size()-1; i++) {
                    drawHelpString("spin me", circles[i]->x, circles[i]->y, yOffset, 0);
                    drawHelpString("backward", circles[i]->x, circles[i]->y, yOffset, 1);
                    /*helpFont.drawString("spin me", circles[i]->x-helpFont.stringWidth("spin me")/2, circles[i]->y+circles[i]->w/2+20*deviceScale);
                    helpFont.drawString("backward", circles[i]->x-helpFont.stringWidth("backward")/2, circles[i]->y+circles[i]->w/2+20*deviceScale + helpTextHeight+4);
                     */
                }
                break;
            case 9:
                drawHelpString("(The white one is", circles[circles.size()-1]->x, circles[circles.size()-1]->y, yOffset, 0);
                drawHelpString("a reverb effect)", circles[circles.size()-1]->x, circles[circles.size()-1]->y, yOffset, 1);
                break;
            case 10:
            case 11:
                arrowLeft.draw(45, screenH-20, 20, 22 );
                x1 = 4+(helpFont.stringWidth("them to move around"))/2;
                y1 = screenH-25-helpTextHeight*2;
                drawHelpString("This button allows", x1, y1, 0, 0);
                drawHelpString("them to move around", x1, y1, 0, 1);
                break;
            case 12:
            case 13:
                arrow.draw(screenW-45, screenH-20, 20, 22 );
                x1 = screenW-(helpFont.stringWidth("(don't leave yet)"))/2-4;
                y1 = screenH-25-helpTextHeight*3;
                drawHelpString("This button", x1, y1, 0, 0);
                drawHelpString("exits to menu", x1, y1, 0, 1);
                drawHelpString("(don't leave yet)", x1, y1, 0, 2);
                /*helpFont.drawString("This button", screenW-helpFont.stringWidth("(don't leave yet)")-4, screenH-70);
                helpFont.drawString("exits to menu", screenW-helpFont.stringWidth("(don't leave yet)")-4, screenH-55);
                helpFont.drawString("(don't leave yet)", screenW-helpFont.stringWidth("(don't leave yet)")-4, screenH-40);*/
                break;
            case 14:
            case 15:
                for (int i=0; i<2; i++) {
                    drawHelpString("Touch two and", circles[i]->x, circles[i]->y, yOffset, 0);
                    drawHelpString("bring together", circles[i]->x, circles[i]->y, yOffset, 1);
                    drawHelpString("to join", circles[i]->x, circles[i]->y, yOffset, 2);
                }
                break;
            case 16:
            case 17:
                drawHelpString("Double tap", circles[1]->x, circles[1]->y, yOffset, 0);
                drawHelpString("while sleeping", circles[1]->x, circles[1]->y, yOffset, 1);
                drawHelpString("to change sound", circles[1]->x, circles[1]->y, yOffset, 2);
                break;
        }
        ofSetColor(255, 255, 255);
    }
}

void ofApp::drawHelpString(string s, int x1, int y1, int yOffset, int row) {
    helpFont.drawString(s, x1 - helpFont.stringWidth(s)/2, y1 + yOffset + helpTextHeight * row) ;
}