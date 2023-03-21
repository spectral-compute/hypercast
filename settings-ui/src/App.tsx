import {useContext} from 'react';
import './App.css';
import {AppContext} from "./index";
import LoadEstimator from "./LoadEstimator";
import PortStatus from "./PortStatus";

function App() {
  const appCtx = useContext(AppContext);

  // TODO: initial state loading crap.


  return <>
      <div className="mainCols">
        <div className="inputsCol col">
          <h2>Inputs</h2>

          {
            appCtx.machineInfo.inputPorts.map(p =>
              <PortStatus desc={p}></PortStatus>
          )}
        </div>
        <div className="streamsCol col">
            <h2>Streams</h2>
        </div>
      </div>
      <LoadEstimator compute={0.7} localBandwidth={10000}></LoadEstimator>
  </>;
}

export default App;
