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

  if (threadShouldExit()) {
    return 1;
  }

  switch (reason) {
    case LWS_CALLBACK_SERVER_WRITEABLE:
      // hrm thread safety ?
      if (!mWebSocketInstance) {
        mWebSocketInstance = wsi;
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
  : Thread("WebSocketThread"),
    mProcessor(aProcessor),
    mWebSocketInstance(nullptr)
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
      mWebSocketInstance = libwebsocket_client_connect_extended(context,
                                                                address,
                                                                port,
                                                                use_ssl,
                                                                uri,
                                                                ads_port,
                                                                ads_port,
                                                                NULL,
                                                                -1,
                                                                nullptr);
      if (!mWebSocketInstance) {
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

void WebSocketServer::Write(const char* aBuffer, uint32_t aLength)
{
  memcpy(&mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], aBuffer, aLength);
  if (mWebSocketInstance) {
    // hrm thread safety ?
    uint32_t n = libwebsocket_write(mWebSocketInstance, &mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], aLength, LWS_WRITE_TEXT);
    if (n == -1) {
      printf("connection lost\n");
      return;
    }
  }
}


TmpSndDawAudioProcessor::TmpSndDawAudioProcessor()
: mWebSocket(new WebSocketServer(this)),
  mState(WAITING_FOR_PARAMS),
  mEditor(nullptr)
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
  return mParameters.size();
}

float TmpSndDawAudioProcessor::getParameter (int aIndex)
{
  return mParameters[aIndex]->mValue;
}

void TmpSndDawAudioProcessor::setParameter (int aIndex, float aValue)
{
  mParameters[aIndex]->mValue = aValue;
  // send that to the socket
  // xxx do param invalidation here so we can send in the cb or something
  char buf[1024];
  sprintf(buf, "%d,%f", aIndex, aValue);
  mWebSocket->Write(buf, strlen(buf));
}

const String TmpSndDawAudioProcessor::getParameterName (int aIndex)
{
  return mParameters[aIndex]->mName;
}

const String TmpSndDawAudioProcessor::getParameterText (int aIndex)
{
  return mParameters[aIndex]->mName;
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
  // check if this works nicely when seeking and looping and such
  AudioPlayHead* playHead = getPlayHead();
  AudioPlayHead::CurrentPositionInfo info;
  playHead->getCurrentPosition(info);
  // that should be the transport time, right ?
  // printf("%f\n", info.editOriginTime + info.timeInSeconds);

  MidiBuffer::Iterator it(midiMessages);
  MidiMessage msg;
  int pos;
  while(it.getNextEvent(msg, pos)) {
    double ts = msg.getTimeStamp();
    bool isNote = msg.isNoteOnOrOff();
    bool isNoteOn = msg.isNoteOn();
    bool isController = msg.isController();
    uint32_t note = msg.getNoteNumber();
    uint32_t channel = msg.getChannel();
    uint8_t velocity = msg.getVelocity();
    ProtocolMessage buf;
    if (isNote) {
      if (isNoteOn) {
        buf = mProtocol.NoteOn(ts, channel, note, velocity);
      } else {
        buf = mProtocol.NoteOff(ts, channel);
      }
    } else if (isController) {
      // mmh ? or just une the daw's curves
      // buf = mProtocol.ParameterChange(ts, channel, )
    } else {
      assert(false && "not implemented");
    }
    mWebSocket->Write(buf.mData, buf.mLength);
  }

  for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
    buffer.clear (i, 0, buffer.getNumSamples());

  for (int channel = 0; channel < getNumInputChannels(); ++channel)
  {
    float* channelData = buffer.getWritePointer (channel);
  }
}

bool TmpSndDawAudioProcessor::hasEditor() const
{
  return true;
}

AudioProcessorEditor* TmpSndDawAudioProcessor::createEditor()
{
  mEditor = new TmpSndDawAudioProcessorEditor(this);
  return mEditor;
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
   *        "step": 1,
   *        "default": 1
   *     },
   *     "param2name": {
   *        "min": 0,
   *        "max": 50,
   *        "step": 10,
   *        "default": 10
   *     },
   *   },
   *   "instrument2Name" : {
   *     "param1name": {
   *        "min": 0,
   *        "max": 10,
   *        "step": 1,
   *        "default": 1
   *     },
   *     "param2name": {
   *        "min": 0,
   *        "max": 50,
   *        "step": 10,
   *        "default": 10
   *     }
   *   },
   *   "sends": {
   *     "reverb" : {
   *       "decay": {
   *         "min": 0.,
   *         "max": 10.,
   *         "step", 0.1,
   *         "default", 5.
   *       }
   *     }
   *   },
   *   "master": {
   *     "compression" : {
   *       "ratio": {
   *         "min": 0.,
   *         "max": 20.,
   *         "step", 0.1,
   *         "default", 4.
   *       }
   *     }
   *   }
   * }
   */
  if (!res.isObject()) {
    return false;
  }

  DynamicObject* obj = res.getDynamicObject();
  NamedValueSet& props (obj->getProperties());

  // find the sends name
  Array<String> sendsNames;
  for (int i = 0; i < props.size(); ++i) {
    const Identifier id (props.getName (i));
    if (id.toString() == "sends") {
      var values(props[id]);
      DynamicObject* sendsObj = values.getDynamicObject();
      NamedValueSet& sendsProps(sendsObj->getProperties());
      for (int j = 0; j < sendsProps.size(); j++) {
        const Identifier id (sendsProps.getName (j));
        sendsNames.add(id.toString());
      }
    }
  }

  printf("Sends:\n");
  for (uint32_t i = 0; i < sendsNames.size(); i++) {
    printf("- %s\n", sendsNames[i].toRawUTF8());
  }

  // instruments
  for (int i = 0; i < props.size(); ++i) {
    const Identifier idInst (props.getName (i));
    // we deal with send and master effects separately
    if (idInst.toString() == "sends" ||
        idInst.toString() == "master") {
      continue;
    }
    var child (props[idInst]);
    if (!child.isObject()) {
      return false;
    }

    DynamicObject* params = child.getDynamicObject();
    NamedValueSet& paramProps (params->getProperties());

    // params
    for (int j = 0; j < paramProps.size(); ++j) {
      Parameter* param = new Parameter();
      const Identifier id (paramProps.getName (j));
      var values(paramProps[id]);
      if (!values.isObject()) {
        return false;
      }
      // name is "instrumentName paramId"
      param->mName = idInst.toString() + " " + id.toString();
      DynamicObject* valuesObj = values.getDynamicObject();
      NamedValueSet& valuesProps(valuesObj->getProperties());

      // min/max/step/default values
      for (int k = 0; k < valuesProps.size(); ++k) {
        String name = valuesProps.getName(k).toString();
        float value = valuesProps.getValueAt(k).toString().getDoubleValue();
        if (name == "min") {
          param->mMin = value;
        } else if (name == "max") {
          param->mMax = value;
        } else if (name == "step") {
          param->mStep = value;
        } else if (name == "default") {
          param->mDefault = value;
        } else {
          assert(false && "invalid key in instrument parameter");
        }
      }
      mParameters.add(param);
    }

    for (uint32_t j = 0; j < sendsNames.size(); j++) {
      Parameter* param = new Parameter();
      param->mName = idInst.toString() + " " + " send " + sendsNames[j];
      param->mMin = 0.0;
      param->mMax = 2.0;
      param->mStep = 0.01;
      param->mDefault= 0.0;
      mParameters.add(param);
    }
  }

  // now, sends and master bus effects
  for (int i = 0; i < props.size(); ++i) {
    const Identifier idEffect(props.getName (i));
    if (idEffect.toString() != "sends" &&
        idEffect.toString() != "master") {
      continue;
    }
    const Identifier idBus(props.getName (i)); // "sends" or "master"
    var child (props[idEffect]);
    if (!child.isObject()) {
      return false;
    }

    DynamicObject* busEffectObj = child.getDynamicObject();
    NamedValueSet& busEffectProps = (busEffectObj->getProperties());
    for (uint32_t j = 0; j < busEffectProps.size(); j++) {
      const Identifier idEffectParam(busEffectProps.getName (j));
      var child (busEffectProps[idEffectParam]);
      if (!child.isObject()) {
        return false;
      }

      Parameter* param = new Parameter();

      DynamicObject* busEffectParamObj = child.getDynamicObject();
      NamedValueSet& busEffectParamProps = (busEffectParamObj->getProperties());
      for (uint32_t k = 0; k < busEffectParamProps.size(); k++) {
        const Identifier idEffectParamValue(busEffectParamProps.getName (k));
        var child (busEffectParamProps[idEffectParamValue]);
        if (!child.isObject()) {
          return false;
        }

        // name is "type sendname sendparamname"
        param->mName = idBus.toString() + " " +
                       idEffectParam.toString() + " " +
                       idEffectParamValue.toString();
        DynamicObject* busEffectParamValueObj = child.getDynamicObject();
        NamedValueSet& busEffectParamValueProps = (busEffectParamValueObj->getProperties());
        for (uint32_t l = 0; l < busEffectParamValueProps.size(); l++) {
          String name = busEffectParamValueProps.getName(l).toString();
          float value = busEffectParamValueProps.getValueAt(l).toString().getDoubleValue();
          if (name == "min") {
            param->mMin = value;
          } else if (name == "max") {
            param->mMax = value;
          } else if (name == "step") {
            param->mStep = value;
          } else if (name == "default") {
            param->mDefault = value;
          } else {
            assert(false && "invalid key in bus parameter values");
          }
        }
      }

      mParameters.add(param);
    }
  }
  String parsed;
  for (uint32_t i = 0; i < mParameters.size(); i++) {
    parsed << mParameters[i]->mName + "\n";
    parsed << "\t" << "min:" << mParameters[i]->mMin << "\n";
    parsed << "\t" << "max:" << mParameters[i]->mMax << "\n";
    parsed << "\t" << "step:" << mParameters[i]->mStep << "\n";
    parsed << "\t" << "default:" << mParameters[i]->mDefault << "\n";
  }
  printf("%s\n", parsed.toRawUTF8());
}

void TmpSndDawAudioProcessor::onReceivedData(const void* aData, size_t aSize)
{
  switch(mState) {
    case WAITING_FOR_PARAMS: {
      bool rv = deserializeParams(aData, aSize);
      if (rv) {
        mState = PROCESSING;
      }
      if (mEditor) {
        mEditor->Initialize();
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

void TmpSndDawAudioProcessor::OnEditorClose()
{
  mEditor = nullptr;
}

Array<Parameter*, CriticalSection>*
TmpSndDawAudioProcessor::GetParametersArray()
{
  return &mParameters;
}

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new TmpSndDawAudioProcessor();
}
