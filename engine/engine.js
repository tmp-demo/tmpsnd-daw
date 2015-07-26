tmpsnd = function(ac) {
  // midi note to frequency
  function n2f(n) {
    return Math.pow(2, (n - 69) / 12) * 440;
  }

  // trigger a note
  this.trigger = function(tuple) {
    var t = tuple[0];
    channels[tuple[1]].play(t /* delta t */,
                           n2f(tuple[2]) /* note */,
                           (tuple[3] - 1) / 127);
  }

  // stop a note
  this.stop = function(tuple) {
    var t = tuple[0];
    if (channels[tuple[1]].stop) {
      channels[tuple[1]].stop(t);
    }
  }

  var parameters = [];

  function registerParameters(param) {
    if (param instanceof AudioParam) {
      parameters.push({value: 0.0, audioparam: param});
    } else if (param instanceof Function) {
      parameters.push({value: 0.0, func: param});
    } else {
      parameters.push(param);
    }
    return parameters.length - 1;
  }

  this.setParameter = function(tuple) {
    if (tuple[1] > parameters.length - 1) {
      console.log("param out of range");
      return;
    }
    if (parameters[tuple[1]].audioparam) { // AudioParam
      parameters[tuple[1]].audioparam.setValueAtTime(tuple[2], tuple[0]);
      parameters[tuple[1]].value = tuple[2];
    } else if (parameters[tuple[1]].func) { // Function
      parameters[tuple[1]].func(tuple[2], tuple[0]);
      parameters[tuple[1]].value = tuple[2];
    } else {                                // Simple value
      parameters[tuple[1]].value = tuple[2];
    }
  }

  function getParameter(index) {
    return parameters[index].value;
  }

  function noiseBuffer() {
    var buf = ac.createBuffer(1, ac.sampleRate * 0.5, ac.sampleRate / 2);
    var c = buf.getChannelData(0);
    for(var i = 0; i < c.length; i++) { 
      c[i] = Math.random() * 2.0 - 1.0; 
    }
    return buf;
  }

  function release(t, param, startValue, endValue, constant) {
    // param.cancelScheduledValues(t);
    param.setValueAtTime(startValue, t);
    param.setTargetAtTime(endValue, t+0.005, constant);
  }

  instruments = [
    function kick(sink) {

      var oc = new OfflineAudioContext(1, 44100, 44100);

      var bodyrelease = {value : 0.26};
      var noiserelease = {value : 0.03};
      var noisemix = {value : 3.2};
      var kickfreq = {value : 45.0};
      var lopass = {value: 16000.0};

      var osc = oc.createOscillator();
      var velocity = oc.createGain();
      var bodyenveloppe = oc.createGain();
      var noiseenveloppe = oc.createGain();
      var lopass = oc.createBiquadFilter();

      bodyenveloppe.gain.setValueAtTime(0, 0);
      noiseenveloppe.gain.setValueAtTime(0, 0);
      osc.frequency.setValueAtTime(0, 0);

      osc.connect(bodyenveloppe);
      noiseenveloppe.connect(lopass);
      bodyenveloppe.connect(velocity);
      lopass.connect(oc.destination);

      var s = oc.createBufferSource();
      s.buffer = noiseBuffer();
      s.connect(noiseenveloppe);
      s.start(0);

      var KICK_RELEASE = registerParameters(bodyrelease);
      var KICK_FREQUENCY = registerParameters(kickfreq);
      var NOISE_RELEASE = registerParameters(noiserelease);
      var NOISE_MIX = registerParameters(noisemix);
      registerParameters(lopass.frequency);

      release(0, bodyenveloppe.gain, 1, 0, getParameter(KICK_RELEASE) / 3);
      release(0, noiseenveloppe.gain, getParameter(NOISE_MIX) * 1.0, 0, getParameter(NOISE_RELEASE));
      release(0, osc.frequency, getParameter(KICK_FREQUENCY) * 2, getParameter(KICK_FREQUENCY), 0.03);
      osc.start(0);


      var outbuffer;
      oc.startRendering().then(function(buf) {
         outbuffer = buf;
      })

      this.play = function(t, freq, vel) {
        var b = ac.createBufferSource();
        var velo = ac.createGain();
        velo.gain.value = vel;
        b.buffer = outbuffer;
        b.start(t);
        b.connect(velo);
        velo.connect(sink);
      }
    },
   function snare(sink) {
     var noiserelease = {value: 0.03};
     var bodyrelease = {value: 0.02};
     var snarepitch = {value: 0.0};
     var lowpass = {value: 2300.0};

     var oc = new OfflineAudioContext(1, 44100, 44100);

     var lopass = oc.createBiquadFilter();
     var osc1 = oc.createOscillator();
     osc1.type = "triangle";
     var osc2 = oc.createOscillator();
     osc2.type = "triangle";
     var osc3 = oc.createOscillator();
     var osc4 = oc.createOscillator();
     var velocity = oc.createGain();

     var enveloppe = oc.createGain();
     var enveloppeBody = oc.createGain();

     var NOISE_RELEASE = registerParameters(noiserelease);
     var BODY_RELEASE = registerParameters(bodyrelease);
     var LOWPASS_CUTOFF = registerParameters(lowpass);
     var SNARE_PITCH = registerParameters(snarepitch);

     enveloppe.gain.setValueAtTime(0, 0);
     enveloppeBody.gain.setValueAtTime(0, 0);

     osc1.connect(enveloppeBody);
     osc2.connect(enveloppeBody);
     osc3.connect(enveloppeBody);
     osc4.connect(enveloppeBody);

     osc1.frequency.setValueAtTime(111 + 175, 0);
     osc2.frequency.setValueAtTime(111 + 224, 0);
     osc3.frequency.setValueAtTime(380, 0);
     osc4.frequency.setValueAtTime(180, 0);

     enveloppeBody.connect(lopass);
     enveloppe.connect(lopass);
     lopass.connect(velocity);
     velocity.connect(oc.destination);
     noisebuffer = noiseBuffer();

     var noise = oc.createBufferSource();
     noise.buffer = noisebuffer;
     noise.connect(enveloppe);
     noise.start(0);
     lopass.frequency.setValueAtTime(getParameter(LOWPASS_CUTOFF), 0);

     velocity.gain.setValueAtTime(1.0, 0);

     release(0, enveloppe.gain, 1, 0, getParameter(NOISE_RELEASE));
     release(0, enveloppeBody.gain, 1, 0, getParameter(BODY_RELEASE));

     osc1.start(0);
     osc2.start(0);
     osc3.start(0);
     osc4.start(0);

      var outbuffer;
      oc.startRendering().then(function(buf) {
         outbuffer = buf;
      });

      this.play = function(t, freq, vel) {
        var b = ac.createBufferSource();
        var velo = ac.createGain();
        velo.gain.value = vel;
        b.buffer = outbuffer;
        b.start(t);
        b.connect(velo);
        velo.connect(sink);
      }

   },
   function hihat(sink) {
     var relhigh = {value: 0.02};
     var relband = {value: 0.01};
     var cutoff = {value: 11800};
     var hipass= {value: 7300};

     var oc = new OfflineAudioContext(1, 44100, 44100);

     var enveloppehigh = oc.createGain();
     var enveloppeband = oc.createGain();
     var bandpass = oc.createBiquadFilter();
     var highpass = oc.createBiquadFilter();
     var velocity = oc.createGain();

     var osc1 = oc.createOscillator();
     var osc2 = oc.createOscillator();
     var osc3 = oc.createOscillator();
     var osc4 = oc.createOscillator();
     var osc5 = oc.createOscillator();
     var osc6 = oc.createOscillator();
     osc1.type = osc2.type = osc3.type = osc4.type = osc5.type = osc6.type = "square";

     enveloppehigh.gain.setValueAtTime(0, 0);
     enveloppeband.gain.setValueAtTime(0, 0);
     bandpass.type = "bandpass";
     highpass.type = "highpass";

     var RELHIGH= registerParameters(relhigh);
     var RELBAND= registerParameters(relband);
     bandpass.frequency.value=11800;
     highpass.frequency.value=7300;
     registerParameters(bandpass.frequency);
     registerParameters(highpass.frequency);
     osc1.frequency.value = 50;
     osc2.frequency.value = 100;
     osc3.frequency.value = 150;
     osc4.frequency.value = 200;
     osc5.frequency.value = 250;
     osc6.frequency.value = 300;
     registerParameters(osc1);
     registerParameters(osc2);
     registerParameters(osc3);
     registerParameters(osc4);
     registerParameters(osc5);
     registerParameters(osc6);

     osc1.connect(bandpass);
     osc2.connect(bandpass);
     osc3.connect(bandpass);
     osc4.connect(bandpass);
     osc5.connect(bandpass);
     osc6.connect(bandpass);
     osc1.connect(highpass);
     osc2.connect(highpass);
     osc3.connect(highpass);
     osc4.connect(highpass);
     osc5.connect(highpass);
     osc6.connect(highpass);
     highpass.connect(enveloppehigh);
     bandpass.connect(enveloppeband);
     enveloppehigh.connect(velocity)
     enveloppeband.connect(velocity)
     velocity.connect(oc.destination);

     velocity.gain.setValueAtTime(1, 0);
     release(0, enveloppehigh.gain, 0.1, 0, getParameter(RELHIGH));
     release(0, enveloppeband.gain, 0.1, 0, getParameter(RELBAND));

     osc1.start();
     osc2.start();
     osc3.start();
     osc4.start();
     osc5.start();
     osc6.start();

      var outbuffer;
      oc.startRendering().then(function(buf) {
         outbuffer = buf;
      });

      this.play = function(t, freq, vel) {
        var b = ac.createBufferSource();
        var velo = ac.createGain();
        velo.gain.value = vel;
        b.buffer = outbuffer;
        b.start(t);
        b.connect(velo);
        velo.connect(sink);
      }
   },
   function substractive(sink) {
     var q = {value: 0.0};
     var detune = {value: 0.0};
     var filterattack = {value: 0.0};
     var attack = {value:0.0};
     var rel= {value:0.0};

     var osc1 = ac.createOscillator();
     osc1.type = "sawtooth";
     var osc2 = ac.createOscillator();
     osc2.type = "sawtooth";
     var filter = ac.createBiquadFilter();
     var enveloppe = ac.createGain();

     var CUTOFF = registerParameters(filter.frequency);
     var Q = registerParameters(filter.Q);
     var DETUNE = registerParameters(detune);
     var FILTER_ATTACK = registerParameters(filterattack);
     var ATTACK = registerParameters(attack);
     var RELEASE = registerParameters(rel);

     osc1.connect(filter);
     osc2.connect(filter);
     filter.connect(enveloppe);
     enveloppe.connect(sink);

     enveloppe.gain.setValueAtTime(0, 0);

     this.play = function(t, freq, vel) {
       osc1.frequency.setValueAtTime(freq, t);
       osc2.frequency.setValueAtTime(freq, t);
       osc2.detune.setValueAtTime(getParameter(DETUNE), t);

       enveloppe.gain.setValueAtTime(0, t);
       enveloppe.gain.linearRampToValueAtTime(vel, t + getParameter(ATTACK));

       filter.frequency.setValueAtTime(0, t);
       filter.frequency.linearRampToValueAtTime(getParameter(CUTOFF), t + getParameter(FILTER_ATTACK));
     }

     this.stop = function(t) {
       enveloppe.gain.setTargetAtTime(0, t, getParameter(RELEASE));
     }

     osc1.start();
     osc2.start();
   },
   function clave(sink) {
     var rel = { value: 0.0 };
     var osc = ac.createOscillator();
     osc.type = "triangle";

     var velocity = ac.createGain();
     var enveloppe = ac.createGain();

     var filter = ac.createBiquadFilter();
     filter.type = "bandpass";

     registerParameters(filter.frequency);
     registerParameters(osc.frequency);
     var CLAVE_RELEASE = registerParameters(rel);

     osc.frequency.setValueAtTime(0, 0);
     filter.frequency.setValueAtTime(0, 0);
     velocity.gain.setValueAtTime(0, 0);
     enveloppe.gain.setValueAtTime(0, 0);

     osc.connect(enveloppe);
     enveloppe.connect(velocity);
     velocity.connect(filter);
     filter.connect(sink);

     this.play = function(t, freq, vel) {
       velocity.gain.setValueAtTime(vel, t);
       release(t, enveloppe.gain, 1, 0, getParameter(CLAVE_RELEASE));
     }
     osc.start();
   },
   function cowbell(sink) {
     var rel = { value: 0.0 };
     var osc1 = ac.createOscillator();
     var osc2 = ac.createOscillator();
     osc1.type = "square";
     osc2.type = "square";

     var velocity = ac.createGain();
     var enveloppe = ac.createGain();

     var filter = ac.createBiquadFilter();
     filter.type = "bandpass";

     registerParameters(filter.frequency);
     registerParameters(osc1.frequency);
     registerParameters(osc2.frequency);
     var cowbell_RELEASE = registerParameters(rel);

     osc1.frequency.setValueAtTime(0, 0);
     osc2.frequency.setValueAtTime(0, 0);
     filter.frequency.setValueAtTime(0, 0);
     velocity.gain.setValueAtTime(0, 0);
     enveloppe.gain.setValueAtTime(0, 0);

     osc1.connect(enveloppe);
     osc2.connect(enveloppe);
     enveloppe.connect(velocity);
     velocity.connect(filter);
     filter.connect(sink);

     this.play = function(t, freq, vel) {
       velocity.gain.setValueAtTime(vel, t);
       release(t, enveloppe.gain, 1, 0, getParameter(cowbell_RELEASE));
     }
     osc1.start();
     osc2.start();
   }
 ]

 channels = [];

 function reverbBuffer(length, decay) {
   var i,l;
   var len = ac.sampleRate * length;
   var buffer = ac.createBuffer(2, len, ac.sampleRate)
     var iL = buffer.getChannelData(0)
     var iR = buffer.getChannelData(1)
     for(i=0,l=buffer.length;i<l;i++) {
       iL[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
       iR[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
     }
   return buffer;
 }

 this.master = ac.createGain();
 this.comp = ac.createDynamicsCompressor();
 this.master.connect(this.comp);
 this.comp.connect(ac.destination);

 var reverb = ac.createConvolver();
 reverb.buffer = reverbBuffer(2, 5);
 reverb.connect(master);

 var delay = ac.createDelay();
 delay.delayTime.setValueAtTime(0,0);
 var delayGain = ac.createGain();
 delay.connect(delayGain);
 delayGain.connect(delay);
 delayGain.connect(master);

 channels.push([]); // 1 indexed
 for (var i in instruments) {
   var volume = ac.createGain();
   registerParameters(volume.gain);
   var send_delay = ac.createGain();
   var send_reverb = ac.createGain();
   channels.push(new instruments[i](volume));
   volume.connect(master);
   volume.connect(send_delay);
   volume.connect(send_reverb);
   send_delay.gain.setValueAtTime(0,0);
   send_reverb.gain.setValueAtTime(1,0);
   send_delay.connect(delay);
   send_reverb.connect(reverb);
   registerParameters(send_delay.gain);
   registerParameters(send_reverb.gain);
 }

 registerParameters(delayGain.gain);
 registerParameters(delay.delayTime);

 var REVERB_LENGTH = registerParameters(function(newValue) {
   reverb.buffer = reverbBuffer(newValue, getParameter(REVERB_DECAY));
 });
 var REVERB_DECAY = registerParameters(function(newValue) {
   reverb.buffer = reverbBuffer(getParameter(REVERB_LENGTH), newValue);
 });

 registerParameters(comp.ratio);
 registerParameters(comp.threshold);
 registerParameters(comp.knee);
 registerParameters(comp.ratio);
 registerParameters(comp.attack);
 registerParameters(comp.release);
 return this;
}
