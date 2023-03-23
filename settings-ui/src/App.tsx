import {useContext} from 'react';
import './App.sass';
import {AppContext} from "./index";
// import LoadEstimator from "./LoadEstimator";
import PortStatus from "./PortStatus";
import StreamBox from "./StreamBox";
import LogoCard from "./LogoCard";
import {ReactComponent as CPU} from "./assets/icons/cpu.svg";
import {ReactComponent as UplinkIcon} from "./assets/icons/upload-cloud.svg";
import {AudioCodec, Channel, FrameRateType, H26xPreset, VideoCodec} from "./api/Config";
import NewChannelButton from "./NewChannelButton";

function App() {
  const appCtx = useContext(AppContext);

  const chan: Channel = {
      name: "Channel 1",
      videoSource: {
          arguments: [],
          latency: 0,
          url: "Herring",
          loop: true,
          timestamp: false
      },

      videoQualities: [{
          targetLatency: 1000,
          interleaveTimestampInterval: 10,
          clientBufferControl: {},
          width: 1920,
          height: 1080,
          frameRate: {
              numerator: 30,
              denominator: 1,
              type: FrameRateType.fps
          },
          bitrate: 10000,
          crf: 5,
          codec: VideoCodec.h264,
          h26xPreset: H26xPreset.fast
      }, {
          targetLatency: 1000,
          interleaveTimestampInterval: 10,
          clientBufferControl: {},
          width: 1280,
          height: 720,
          frameRate: {
              numerator: 30,
              denominator: 1,
              type: FrameRateType.fps
          },
          bitrate: 10000,
          crf: 5,
          codec: VideoCodec.h264,
          h26xPreset: H26xPreset.fast
      }],
      audioQualities: [{
          bitrate: 1000,
          codec: AudioCodec.aac
      }, {
          bitrate: 1000,
          codec: AudioCodec.aac
      }]
  };

  const channels: Channel[] = [chan, chan];

  // TODO: initial state loading crap.
  return <>
      <div className="layout">
          <div className="topRow">
              <LogoCard/>
              <div className="portList">
                  {
                      appCtx.machineInfo.inputPorts.map(p =>
                          <PortStatus connected={p.connectedMediaInfo != null} desc={p}></PortStatus>
                  )}
              </div>

              <div className="statList">
                  <div className="statEntry">
                      <span>1MB/s</span>
                      <UplinkIcon/>
                  </div>

                  <div className="statEntry">
                      <span>75%</span>
                      <CPU/>
                  </div>
              </div>
          </div>

          <div className="streams">
              {channels.map((c) => <StreamBox config={c}></StreamBox>)}
              <NewChannelButton clicked={() => {}}/>
          </div>
      </div>

      {/*<LoadEstimator compute={0.7} localBandwidth={10000}></LoadEstimator>*/}
  </>;
}

export default App;
