import $ from 'jquery';
import {useState, useEffect} from 'react';
import { Routes, Route, Outlet } from 'react-router-dom';
import '../Admin.css';
import NavBar from './NavBar';


function AdminPage() {
  const [nodes, setNodes] = useState([]);

  useEffect(() => {
      $.ajax({
        url: `http://localhost:8000/admin`,
        method: 'GET',
        crossDomain: true,
        dataType: 'json',
        success: (res) => {
            setNodes(res.data);
        }
      });
  }, []);

  function changeNodeStatus(e, item) {
      console.log(item);

      $.ajax({
        url: 'http://localhost:8000/node_toggle',
        method: 'POST',
        crossDomain: true,
        dataType: 'json',
        data: JSON.stringify(item),
        success: (_) => {
        }
      });
  }

  return (
      <>
        <NavBar />
        <div className="header">Frontend Nodes</div>
        <div className="wrap">
            {nodes.map((item, index) =>
                (item.is_frontend === "true") ?
                <div className="container node" key={index}>
                    <div>Node Address: {item.nodeID}</div>
                    <div>STATUS: {(item.is_alive === "true") ?
                                   <span className="up_label">UP</span> :
                                   <span className="down_label">DOWN</span>}
                    </div>
                    <button className="down_btn"
                        onClick={(e) => changeNodeStatus(e, item)}>
                        Toggle
                    </button>
                </div>
                :
                <></>
            )}
        </div>
        <div className="header">Storage Nodes</div>
        <div className="wrap">
            {nodes.map((item, index) =>
                (item.is_frontend !== "true") ?
                <div className="container node" key={index}>
                    <div>Node Address: {item.nodeID}</div>
                    <div>STATUS: {(item.is_alive === "true") ?
                                   <span className="up_label">UP</span> :
                                   <span className="down_label">DOWN</span>}
                    </div>
                    <button className="down_btn"
                        onClick={(e) => changeNodeStatus(e, item)}>
                        Toggle
                    </button>
                </div>
                :
                <></>
            )}
        </div>
      </>
  );
}

export default AdminPage;
