#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "libwebsockets.h"
#include "Protocol.h"

class TmpSndDawAudioProcessor;
class TmpSndDawAudioProcessorEditor;

const size_t MAX_PAYLOAD = 65536;

class WebSocketServer : public Thread
{
public:
  WebSocketServer(TmpSndDawAudioProcessor* aProcessor);
  virtual ~WebSocketServer();
  virtual void run() override;

  struct SessionData {
    unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING +
                      MAX_PAYLOAD +
                      LWS_SEND_BUFFER_POST_PADDING];
    unsigned int len;
    unsigned int index;
  };

  void Write(const char* aBuffer, uint32_t aLength);

  static int
  S_Callback(struct libwebsocket_context *context,
             struct libwebsocket *wsi,
             enum libwebsocket_callback_reasons reason, void *user,
             void *in, size_t len);
private:
  // weak, always valid
  TmpSndDawAudioProcessor* mProcessor;
  int
  Callback(struct libwebsocket_context *context,
           struct libwebsocket *wsi,
           enum libwebsocket_callback_reasons reason,
           void *in, size_t len);
  SessionData mSessionData;

  struct libwebsocket * mWebSocketInstance;
  struct libwebsocket_context * mWebSocketContext;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebSocketServer)
};

struct Parameter
{
  String mName;
  float mValue;
  float mMin;
  float mMax;
  float mStep;
  float mDefault;
};

class TmpSndDawAudioProcessor  : public AudioProcessor
{
public:

    enum State {
     /* first, params need to be received from the socket, as a JSON,
      * deserialized and loaded */
      WAITING_FOR_PARAMS,
     /* then, processing happens, only writes from the plugin to the socket
      * happen */
      PROCESSING
    };

    TmpSndDawAudioProcessor();
    ~TmpSndDawAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (AudioSampleBuffer&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    void OnEditorClose();

    const String getName() const override;

    int getNumParameters() override;
    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;

    const String getParameterName (int index) override;
    const String getParameterText (int index) override;

    const String getInputChannelName (int channelIndex) const override;
    const String getOutputChannelName (int channelIndex) const override;
    bool isInputChannelStereoPair (int index) const override;
    bool isOutputChannelStereoPair (int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool deserializeParams(const void* aData, size_t aSize);
    void onReceivedData(const void* aData, size_t aSize);
    void setState(State aState);
    Array<Parameter*, CriticalSection>* GetParametersArray();

private:
    WebSocketServer* mWebSocket;
    State mState;
    Array<Parameter*, CriticalSection> mParameters;
    Array<bool, CriticalSection> mParameterChanged;
    TmpSndDawAudioProcessorEditor* mEditor;
    Protocol mProtocol;
    CriticalSection mLock;
    bool mNeedResetWebSocketServer;
	ScopedPointer<FileOutputStream> mLog;
	File mLogFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TmpSndDawAudioProcessor)
};


#endif  // PLUGINPROCESSOR_H_INCLUDED
