#include "PluginProcessor.h"
#include "PluginEditor.h"

static WebSocketServer* sServer = nullptr;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pluginterfaces/vst2.x/aeffectx.h"


#ifndef _WIN32
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
  return sServer->Callback(context, wsi, reason, in, len);
}

int
WebSocketServer::Callback(struct libwebsocket_context *context,
                struct libwebsocket *wsi,
                enum libwebsocket_callback_reasons reason,
                void *in, size_t len)
{
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
    case LWS_CALLBACK_CLOSED:
      printf("connection closed, clearing state\n");
      mProcessor->setState(TmpSndDawAudioProcessor::WAITING_FOR_PARAMS);
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
  libwebsocket_cancel_service(mWebSocketContext);
  libwebsocket_context_destroy(mWebSocketContext);
}

void WebSocketServer::run()
{
  int n = 0;
  int port = 7681;
  int opts = 0;
  const char *interface = NULL;
  int client = 0;
  int listen_port = 80;
  struct lws_context_creation_info info;
  char uri[256] = "/";
  char address[256], ads_port[256 + 30];

  sServer = this;

  int debug_level = 7;

  memset(&info, 0, sizeof info);

  listen_port = port;

  info.port = listen_port;
  info.iface = interface;
  info.protocols = protocols;
  info.extensions = libwebsocket_get_internal_extensions();
  info.gid = -1;
  info.uid = -1;
  info.options = opts;

  mWebSocketContext = libwebsocket_create_context(&info);
  if (mWebSocketContext == NULL) {
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
      mWebSocketInstance = libwebsocket_client_connect_extended(mWebSocketContext,
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
    n = libwebsocket_service(mWebSocketContext, 10);
  }
#ifdef WIN32
#else
    closelog();
#endif
}

bool WebSocketServer::Write(const char* aBuffer, uint32_t aLength)
{
  memcpy(&mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], aBuffer, aLength);
  if (mWebSocketInstance) {
    // hrm thread safety ?
    uint32_t n = libwebsocket_write(mWebSocketInstance, &mSessionData.buf[LWS_SEND_BUFFER_PRE_PADDING], aLength, LWS_WRITE_TEXT);
    if (n == -1) {
      mProcessor->setState(TmpSndDawAudioProcessor::WAITING_FOR_PARAMS);
      return false;
    }
  }
  return true;
}


TmpSndDawAudioProcessor::TmpSndDawAudioProcessor()
	: mWebSocket(new WebSocketServer(this)),
	mState(WAITING_FOR_PARAMS),
	mEditor(nullptr),
	mNeedResetWebSocketServer(false)
{
	mLogFile = File::getSpecialLocation(File::userHomeDirectory).getChildFile("tmpsnd.snd").getNonexistentSibling();
	mLogFile.create();
	mLog = mLogFile.createOutputStream();
}

TmpSndDawAudioProcessor::~TmpSndDawAudioProcessor()
{ 
  ScopedLock lock(mLock);
  delete mWebSocket;
  mLog->flush();
}

const String TmpSndDawAudioProcessor::getName() const
{
  return JucePlugin_Name;
}

int TmpSndDawAudioProcessor::getNumParameters()
{
  return 100;
}

float TmpSndDawAudioProcessor::getParameter (int aIndex)
{
  ScopedLock lock(mLock);
  if (aIndex > mParameters.size() - 1) {
	return 0.0f;
  }
  return mParameters[aIndex]->mValue;
}

void TmpSndDawAudioProcessor::setParameter (int aIndex, float aValue, bool aFromDaw)
{
  ScopedLock lock(mLock);
  if (aIndex > mParameters.size() - 1) {
	return;
  }
  // The DAW sends param that are in [0,1], rescale to the right domain
  if (aFromDaw) {
	  mParameters[aIndex]->mValue = aValue * (mParameters[aIndex]->mMax - mParameters[aIndex]->mMin) + mParameters[aIndex]->mMin;
  } else {
	  mParameters[aIndex]->mValue = aValue;
  }
  mParameterChanged.set(aIndex, true);
  // Only update the UI if this call was from the DAW, otherwise, the UI is already up-to-date
  if (aFromDaw) {
	  MessageManager::callAsync([=] {
		  ScopedLock lock(mLock);
		  if (mEditor) {
			  if (aIndex < mParameters.size()) {
				  mEditor->setParameter(aIndex, mParameters[aIndex]->mValue);
			  }
		  }
	  });
  }
}


const String TmpSndDawAudioProcessor::getParameterName (int aIndex)
{
  ScopedLock lock(mLock);
  if (aIndex > mParameters.size() - 1) {
    return String("---");
  }

  return mParameters[aIndex]->mName;
}

const String TmpSndDawAudioProcessor::getParameterText (int aIndex)
{
	ScopedLock lock(mLock);
	if (aIndex > mParameters.size() - 1) {
		return String("---");
	}

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
  double transportTime = info.editOriginTime + info.timeInSeconds;

  if (mNeedResetWebSocketServer) {
    ScopedLock lock(mLock);
    delete mWebSocket;
    mWebSocket = new WebSocketServer(this);
    mNeedResetWebSocketServer = false;
	return;
  }


  // param changes first so notes have the right values
  ProtocolMessage buf;
  ScopedLock lock(mLock);
  for (uint32_t i = 0; i < mParameterChanged.size(); i++) {
    if (mParameterChanged[i]) {
      buf = mProtocol.ParameterChange(0 /* now */, i, mParameters[i]->mValue);
      bool success = mWebSocket->Write(buf.mData, buf.mLength);
	  // client disconnected, bail out.
	  if (!success) {
		  return;
	  }
	  buf = mProtocol.ParameterChange(transportTime, i, mParameters[i]->mValue);
	  mLog->writeText(String::formatted("%f,%d,%f\n", (float)transportTime, i, mParameters[i]->mValue), false, false);
      mParameterChanged.set(i, false);
    }
  }

  MidiBuffer::Iterator it(midiMessages);
  MidiMessage msg;
  int pos;
  while(it.getNextEvent(msg, pos)) {
	// convert to seconds offset from the beginning of the callback
    double ts = msg.getTimeStamp() / getSampleRate();
    bool isNote = msg.isNoteOnOrOff();
    bool isNoteOn = msg.isNoteOn();
    bool isController = msg.isController();
    uint32_t note = msg.getNoteNumber();
    uint32_t channel = msg.getChannel();
    uint8_t velocity = msg.getVelocity();
    ProtocolMessage bufrt;
	ProtocolMessage buffile;

    if (isNote) {
      if (isNoteOn) {
        bufrt = mProtocol.NoteOn(ts, channel, note, velocity);
		mLog->writeText(String::formatted("%f,%d,%d,%d\n", (float)transportTime + ts, channel, note, velocity), false, false);

      } else {
        bufrt = mProtocol.NoteOff(ts, channel);
		mLog->writeText(String::formatted("%f,%d\n", (float)transportTime + ts, channel), false, false);

      }
    } else if (isController) {
      // mmh ? or just une the daw's curves
      // buf = mProtocol.ParameterChange(ts, channel, )
    } else {
//      assert(false && "not implemented");
    }
    {
      ScopedLock lock(mLock);
      mWebSocket->Write(bufrt.mData, bufrt.mLength);
    }
  }

  // We don't output sound here, we're just being polite.
  for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i) {
	  buffer.clear(i, 0, buffer.getNumSamples());
  }
}

bool TmpSndDawAudioProcessor::hasEditor() const
{
  return true;
}

AudioProcessorEditor* TmpSndDawAudioProcessor::createEditor()
{
	ScopedLock lock(mLock);
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
  ScopedLock lock(mLock);
  String str(CharPointer_UTF8(static_cast<const char*>(aData)), aSize);
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

//  printf("Sends:\n");
//  for (uint32_t i = 0; i < sendsNames.size(); i++) {
//    printf("- %s\n", sendsNames[i].toRawUTF8());
//  }

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
          param->mDefault = param->mValue = value;
        } else {
          assert(false && "invalid key in instrument parameter");
        }
      }
      mParameters.add(param);
      mParameterChanged.add(true);
    }

    for (uint32_t j = 0; j < sendsNames.size(); j++) {
      Parameter* param = new Parameter();
      param->mName = idInst.toString() + " " + " send " + sendsNames[j];
      param->mMin = 0.0;
      param->mMax = 2.0;
      param->mStep = 0.01;
      param->mValue = param->mDefault= 0.0;
      mParameters.add(param);
      mParameterChanged.add(true);
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


      DynamicObject* busEffectParamObj = child.getDynamicObject();
      NamedValueSet& busEffectParamProps = (busEffectParamObj->getProperties());
      for (uint32_t k = 0; k < busEffectParamProps.size(); k++) {
		Parameter* param = new Parameter();
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
            param->mDefault = param->mValue = value;
          } else {
            assert(false && "invalid key in bus parameter values");
          }
        }
		mParameters.add(param);
		mParameterChanged.add(true);
      }
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
  // printf("%s\n", parsed.toRawUTF8());
    return true;
}

void TmpSndDawAudioProcessor::setState(State aState)
{
  if (mState == PROCESSING &&
      aState == WAITING_FOR_PARAMS) {
    ScopedLock lock(mLock);
    mParameters.clear();
    mParameterChanged.clear();
    if (mEditor) {
      MessageManager::callAsync([=] {
        mEditor->reset();
      });
    }
    mNeedResetWebSocketServer = true;
  }
  mState = aState;
}

void TmpSndDawAudioProcessor::onReceivedData(const void* aData, size_t aSize)
{
  switch(mState) {
    case WAITING_FOR_PARAMS: {
      bool rv = deserializeParams(aData, aSize);
      if (rv) {
        mState = PROCESSING;
      }
      MessageManager::callAsync([=] {
		  ScopedLock lock(mLock);
		if (mEditor) {
          mEditor->Initialize();
		}
      });
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
	ScopedLock lock(mLock);
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
