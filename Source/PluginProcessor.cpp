#include "PluginProcessor.h"
#include "PluginEditor.h"

static WebSocketServer* sServer = nullptr;

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>

#ifndef _WIN32
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#endif

static volatile int force_exit = 0;
static int state;

static struct libwebsocket_protocols protocols[] = {
  { "tmpsnd", WebSocketServer::S_Callback, sizeof(struct WebSocketServer) },
  { NULL, NULL, 0 }
};

int
WebSocketServer::S_Callback(struct libwebsocket_context *context,
                struct libwebsocket *wsi,
                enum libwebsocket_callback_reasons reason, void *user,
                void *in, size_t len)
{
  sServer->Callback(context, wsi, reason, in, len);
}

int
WebSocketServer::Callback(struct libwebsocket_context *context,
                struct libwebsocket *wsi,
                enum libwebsocket_callback_reasons reason,
                void *in, size_t len)
{
  int n;

  printf("callback\n");

  if (threadShouldExit()) {
    return 1;
  }

  switch (reason) {
    case LWS_CALLBACK_SERVER_WRITEABLE:
      printf("writing: %s\n", &mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING]);
      n = libwebsocket_write(wsi, &mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], mSessionData.len, LWS_WRITE_TEXT);
      if (n < 0) {
        lwsl_err("ERROR %d writing to socket, hanging up\n", n);
        return 1;
      }
      if (n < (int)mSessionData.len) {
        lwsl_err("Partial write\n");
        return -1;
      }
      break;

    case LWS_CALLBACK_RECEIVE:
      if (len > MAX_PAYLOAD) {
        lwsl_err("Server received packet bigger than %u, hanging up\n", MAX_PAYLOAD);
        return 1;
      }
      memcpy(&mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], in, len);
      mSessionData.len = (unsigned int)len;

      mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING + len + 1] = 0;

      mProcessor->onReceivedData(&mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], len+1);

      libwebsocket_callback_on_writable(context, wsi);
      break;
    default:
      break;
  }

  return 0;
}


WebSocketServer::WebSocketServer(TmpSndDawAudioProcessor* aProcessor)
: Thread("WebSocketThread"), mProcessor(aProcessor)
{
  force_exit = 0;
  startThread();
}

WebSocketServer::~WebSocketServer()
{
  force_exit = 1;
  stopThread(1000);
}

void WebSocketServer::run()
{
  int n = 0;
  int port = 7681;
  struct libwebsocket_context *context;
  int opts = 0;
  const char *interface = NULL;
#ifndef WIN32
  int syslog_options = LOG_PID | LOG_PERROR;
#endif
  int client = 0;
  int listen_port = 80;
  struct lws_context_creation_info info;
  char uri[256] = "/";
  char address[256], ads_port[256 + 30];
  struct libwebsocket *wsi;

  sServer = this;

  int debug_level = 7;

  memset(&info, 0, sizeof info);

  /* we will only try to log things according to our debug_level */
  setlogmask(LOG_UPTO (LOG_DEBUG));
  openlog("lwsts", syslog_options, LOG_DAEMON);

  /* tell the library what debug level to emit and to send it to syslog */
  lws_set_log_level(debug_level, lwsl_emit_syslog);
  listen_port = port;

  info.port = listen_port;
  info.iface = interface;
  info.protocols = protocols;
  info.extensions = libwebsocket_get_internal_extensions();
  info.gid = -1;
  info.uid = -1;
  info.options = opts;

  context = libwebsocket_create_context(&info);
  if (context == NULL) {
    lwsl_err("libwebsocket init failed\n");
    return;
  }

  n = 0;
  while (n >= 0 && !force_exit) {
    if (client && !state) {
      state = 1;
      /* we are in client mode */

      address[sizeof(address) - 1] = '\0';
      sprintf(ads_port, "%s:%u", address, port & 65535);

      int use_ssl = 0;
      wsi = libwebsocket_client_connect_extended(context,
                                                 address,
                                                 port,
                                                 use_ssl,
                                                 uri,
                                                 ads_port,
                                                 ads_port,
                                                 NULL,
                                                 -1,
                                                 nullptr);
      if (!wsi) {
        lwsl_err("Client failed to connect to %s:%u\n", address, port);
        abort();
      }
    }
    n = libwebsocket_service(context, 10);
  }
  libwebsocket_context_destroy(context);
#ifdef WIN32
#else
    closelog();
#endif
}


TmpSndDawAudioProcessor::TmpSndDawAudioProcessor()
: mWebSocket(new WebSocketServer(this)),
  mState(WAITING_FOR_PARAMS)
{
}

TmpSndDawAudioProcessor::~TmpSndDawAudioProcessor()
{
  delete mWebSocket;
}

const String TmpSndDawAudioProcessor::getName() const
{
  return JucePlugin_Name;
}

int TmpSndDawAudioProcessor::getNumParameters()
{
  return 0;
}

float TmpSndDawAudioProcessor::getParameter (int index)
{
  return 0.0f;
}

void TmpSndDawAudioProcessor::setParameter (int index, float newValue)
{
}

const String TmpSndDawAudioProcessor::getParameterName (int index)
{
  return String();
}

const String TmpSndDawAudioProcessor::getParameterText (int index)
{
  return String();
}

const String TmpSndDawAudioProcessor::getInputChannelName (int channelIndex) const
{
  return String (channelIndex + 1);
}

const String TmpSndDawAudioProcessor::getOutputChannelName (int channelIndex) const
{
  return String (channelIndex + 1);
}

bool TmpSndDawAudioProcessor::isInputChannelStereoPair (int index) const
{
  return true;
}

bool TmpSndDawAudioProcessor::isOutputChannelStereoPair (int index) const
{
  return true;
}

bool TmpSndDawAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool TmpSndDawAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool TmpSndDawAudioProcessor::silenceInProducesSilenceOut() const
{
  return false;
}

double TmpSndDawAudioProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int TmpSndDawAudioProcessor::getNumPrograms()
{
  return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
  // so this should be at least 1, even if you're not really implementing programs.
}

int TmpSndDawAudioProcessor::getCurrentProgram()
{
  return 0;
}

void TmpSndDawAudioProcessor::setCurrentProgram (int index)
{
}

const String TmpSndDawAudioProcessor::getProgramName (int index)
{
  return String();
}

void TmpSndDawAudioProcessor::changeProgramName (int index, const String& newName)
{
}

void TmpSndDawAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
  // Use this method as the place to do any pre-playback
  // initialisation that you need..
}

void TmpSndDawAudioProcessor::releaseResources()
{
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

void TmpSndDawAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
  // In case we have more outputs than inputs, this code clears any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // I've added this to avoid people getting screaming feedback
  // when they first compile the plugin, but obviously you don't need to
  // this code if your algorithm already fills all the output channels.
  for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
    buffer.clear (i, 0, buffer.getNumSamples());

  // This is the place where you'd normally do the guts of your plugin's
  // audio processing...
  for (int channel = 0; channel < getNumInputChannels(); ++channel)
  {
    float* channelData = buffer.getWritePointer (channel);

    // ..do something to the data...
  }
}

bool TmpSndDawAudioProcessor::hasEditor() const
{
  return true;
}

AudioProcessorEditor* TmpSndDawAudioProcessor::createEditor()
{
  return new TmpSndDawAudioProcessorEditor (*this);
}

void TmpSndDawAudioProcessor::getStateInformation (MemoryBlock& destData)
{
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
}

void TmpSndDawAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
  // You should use this method to restore your parameters from this memory block,
  // whose contents will have been created by the getStateInformation() call.
}

bool TmpSndDawAudioProcessor::deserializeParams(const void* aData, size_t aSize)
{
  var res;
  String str(static_cast<const char*>(aData), aSize);
  if (aSize == 0) {
    return false;
  }
  if (!JSON::parse(str, res).wasOk()) {
    return false;
  }
  /* {
   *   "instrument1Name" : {
   *     "param1name": {
   *        "min": 0,
   *        "max": 10,
   *        "step": 1
   *     },
   *     "param2name": {
   *        "min": 0,
   *        "max": 50,
   *        "step": 10
   *     },
   *   },
   *   "instrument2Name" : {
   *     "param1name": {
   *        "min": 0,
   *        "max": 10,
   *        "step": 1
   *     },
   *     "param2name": {
   *        "min": 0,
   *        "max": 50,
   *        "step": 10
   *     }
   *   }
   * }
   */
  if (!res.isObject()) {
    return false;
  }

  String parsed;
  DynamicObject* obj = res.getDynamicObject();
  NamedValueSet& props (obj->getProperties());

  // instruments
  for (int i = 0; i < props.size(); ++i) {
    const Identifier id (props.getName (i));
    parsed << props.getName(i).toString() << "\n";
    var child (props[id]);
    if (!child.isObject()) {
      return false;
    }

    DynamicObject* params = child.getDynamicObject();
    NamedValueSet& paramProps (params->getProperties());

    // params
    for (int i = 0; i < paramProps.size(); ++i) {
      const Identifier id (paramProps.getName (i));
      parsed << "\t";
      parsed << paramProps.getName(i).toString();
      parsed << ": ";
      parsed << paramProps.getValueAt(i).toString().getDoubleValue() << "\n";
    }
  }
  printf(">> %s\n", parsed.toRawUTF8());
}

void TmpSndDawAudioProcessor::onReceivedData(const void* aData, size_t aSize)
{
  switch(mState) {
    case WAITING_FOR_PARAMS: {
      bool rv = deserializeParams(aData, aSize);
      if (rv) {
        mState = PROCESSING;
      }
      break;
    }
    case PROCESSING:
      break;
    default:
      assert(false && "not handled?");
      break;
  }
}

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new TmpSndDawAudioProcessor();
}
