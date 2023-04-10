import {useContext, useState} from 'react';
import './App.sass';
import {AppContext} from "./index";
import PortStatus from "./PortStatus";
import StreamBox from "./StreamBox";
import LogoCard from "./LogoCard";
import {ReactComponent as CPU} from "./assets/icons/cpu.svg";
import {ReactComponent as UplinkIcon} from "./assets/icons/upload-cloud.svg";
import {Channel} from "./api/Config";
import NewChannelButton from "./NewChannelButton";
import ChannelConfigModal from "./modal/ChannelConfigModal";
import {useAsyncDeferred, useAsyncImmediateEx} from './hooks/useAsync';
import Kaput from './Kaput';
import {DECKLINK_PORTS_ORDERED, makeDefaultChannel} from './Constants';
import { observer } from 'mobx-react-lite';
import Loading from "./Loading";


export enum InputPortStatus {
    DISCONNECTED,
    AVAILABLE
}


export default observer(() => {
  const appCtx = useContext(AppContext);

  let [modalOpen, setModalOpen] = useState<boolean>(false);
  let [isNewChannel, setIsNewChannel] = useState<boolean>(false);
  let [channelBeingEdited, setChannelBeingEdited] = useState<[string, Channel] | null>(null);

  const loadCfg = useAsyncImmediateEx(appCtx.loadConfig, {onError: async(_args, e) => {
      if (e.message == "Failed to fetch") {
          setTimeout(() => loadCfg.run(), 1000);
      }
  }});
  const initialProbe = useAsyncImmediateEx(appCtx.probeSDIPorts, {
      onComplete: async() => {
          appCtx.startPollingPorts();
      }
  });
  const saveCfg = useAsyncDeferred(appCtx.saveConfig);
  const loading = loadCfg.isLoading || saveCfg.isLoading || initialProbe.isLoading;

  if (loading) {
      return <Loading/>;
  }

  if (loadCfg.error && loadCfg.error!.message == "Failed to fetch") {
      // Special case: server isn't up yet.
      return <Loading/>;
  }

  if (loadCfg.error || saveCfg.error) {
    return <Kaput message={"Failed to communicate with server process!"}></Kaput>;
  }

  const channels = appCtx.loadedConfiguration.channels;

  function openChannelModal(name: string) {
    const c = appCtx.loadedConfiguration.channels[name]!;
    setChannelBeingEdited([name, JSON.parse(JSON.stringify(c))]);
    setModalOpen(true);
    setIsNewChannel(false);
  }

  function openNewChannelModal(name: string) {
      setChannelBeingEdited([name, makeDefaultChannel(appCtx.getAvailableInputPort()!)]);
      setModalOpen(true);
      setIsNewChannel(true);
  }

  function nameNewChannel() {
      // Just use a number for the moment.
      const keys = Object.keys(channels);
      let n = keys.length;
      while (channels["live/Channel-" + n] != null) {
          n++;
      }

      return String("live/Channel-" + n);
  }

  function saveChannel(name: string, newValue: Channel) {
      appCtx.loadedConfiguration.channels[name] = newValue;
      saveCfg.run(appCtx.loadedConfiguration);
  }

  function deleteChannel(name: string) {
      delete appCtx.loadedConfiguration.channels[name];
      saveCfg.run(appCtx.loadedConfiguration);
  }

  // TODO: initial state loading crap.
  return <>
      <div className="layout">
          <div className="topRow">
              <LogoCard/>
              <div className="portList">
                  {
                      DECKLINK_PORTS_ORDERED.map(p =>
                          <PortStatus
                              key={p}
                              port={p}
                              desc={appCtx.machineInfo.inputPorts[p]}
                          ></PortStatus>
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
              {Array.from(Object.entries(channels)).map(([k, c]) =>
                  <StreamBox
                      key={k}
                      onClick={() => openChannelModal(k)}
                      name={k}
                      config={c}
                  ></StreamBox>)}
              {appCtx.getAvailableInputPort() != null ?
                <NewChannelButton
                    clicked={() => {openNewChannelModal(nameNewChannel());}}
                /> : null
              }
          </div>
      </div>

      {modalOpen ? <ChannelConfigModal
          channel={channelBeingEdited![1]}
          channelName={channelBeingEdited![0]}
          showDeleteBtn={!isNewChannel}
          onClose={() => setModalOpen(false)}
          onSave={saveChannel}
          onDelete={deleteChannel}
      /> : null}

      {/*<LoadEstimator compute={0.7} localBandwidth={10000}></LoadEstimator>*/}
  </>;
});
