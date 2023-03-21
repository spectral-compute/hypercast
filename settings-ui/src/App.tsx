import {useContext} from 'react';
import './App.sass';
import {AppContext} from "./index";
import LoadEstimator from "./LoadEstimator";
import PortStatus from "./PortStatus";
import BoxThing from "./BoxThing";
import StreamBox from "./StreamBox";
import LogoCard from "./LogoCard";

function App() {
  const appCtx = useContext(AppContext);

  // TODO: initial state loading crap.
  return <>
      <LogoCard></LogoCard>
      <div className="topRow">
          <BoxThing title="Inputs">
              <div className="portList">
                  {
                      appCtx.machineInfo.inputPorts.map(p =>
                          <PortStatus connected={p.connectedMediaInfo != null} desc={p}></PortStatus>
                  )}
              </div>
          </BoxThing>

          <BoxThing title="Status">
              <div className="portList">
                  <div className="statEntry">
                      <span>Uplink Usage</span>
                      <span>1MB/s</span>
                  </div>

                  <div className="statEntry">
                      <span>CPU Usage</span>
                      <span>75%</span>
                  </div>
              </div>
          </BoxThing>
      </div>

      <BoxThing title="Streams">
          <div className="streams">
              <StreamBox></StreamBox>
              <StreamBox></StreamBox>
              <StreamBox></StreamBox>
              <StreamBox></StreamBox>
          </div>
      </BoxThing>

      <LoadEstimator compute={0.7} localBandwidth={10000}></LoadEstimator>
  </>;
}

export default App;
