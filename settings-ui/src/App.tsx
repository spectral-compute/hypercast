import {useContext, useState} from 'react';
import './App.sass';
import {AppContext} from "./index";
// import LoadEstimator from "./LoadEstimator";
import PortStatus from "./PortStatus";
import StreamBox from "./StreamBox";
import LogoCard from "./LogoCard";
import {ReactComponent as CPU} from "./assets/icons/cpu.svg";
import {ReactComponent as UplinkIcon} from "./assets/icons/upload-cloud.svg";
import {Channel} from "./api/Config";
import NewChannelButton from "./NewChannelButton";
import ChannelConfigModal from "./modal/ChannelConfigModal";


function App() {
  const appCtx = useContext(AppContext);

  let [modalOpen, setModalOpen] = useState<boolean>(false);
  let [channelBeingEdited, setChannelBeingEdited] = useState<[string, Channel] | null>(null);

  const channels = appCtx.loadedConfiguration.channels;

  function openChannelModal(name: string) {
    const c = appCtx.loadedConfiguration.channels[name]!;
    setChannelBeingEdited([name, JSON.parse(JSON.stringify(c))]);
    setModalOpen(true);
  }

  function saveChannel(name: string, newValue: Channel) {
      console.log("New channel");
      console.log(newValue);
      appCtx.loadedConfiguration.channels[name] = newValue;
      console.log(appCtx.loadedConfiguration);
  }

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
              {Array.from(Object.entries(channels)).map((c) =>
                  <StreamBox
                      onClick={() => openChannelModal(c[0])}
                      name={c[0]}
                      config={c[1]}
                  ></StreamBox>)}
              <NewChannelButton clicked={() => {setModalOpen(true);}}/>
          </div>
      </div>

      {modalOpen ? <ChannelConfigModal
          channel={channelBeingEdited![1]}
          channelName={channelBeingEdited![0]}
          onClose={() => setModalOpen(false)}
          onSave={saveChannel}
      /> : null}

      {/*<LoadEstimator compute={0.7} localBandwidth={10000}></LoadEstimator>*/}
  </>;
}

export default App;
