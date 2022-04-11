import $ from 'jquery';
import {useState, useEffect} from 'react';
import { Routes, Route, Outlet } from 'react-router-dom';
import '../App.css';
import EmailRow from './EmailRow';
import EmailView from './EmailView';
import ComposeEmail from './ComposeEmail';
import NavBar from './NavBar';

function EmailsPage(props) {
  const [data, setData] = useState([]);
  const [checked, setChecked] = useState(new Set());

  // Only gets called when page is first loaded
  useEffect(() => {
      $.ajax({
        url: `http://localhost:8000/${props.sent_mode ? 'sent_emails' : 'emails'}?idx=0&count=80`,
        method: 'GET',
        xhrFields: {
            withCredentials: true,
        },
        crossDomain: true,
        dataType: 'json',
        success: (res) => {
            setData(res.data);
        }
      });
  }, []);

  function onDelete() {
      let arr = []
      for (let x of checked) {
        arr.push(x);
      }
      $.ajax({
        url: 'http://localhost:8000/delete_email',
        method: 'POST',
        crossDomain: true,
        dataType: 'json',
        xhrFields: {
            withCredentials: true,
        },
        data: JSON.stringify({"hashes": arr}),
        success: (_) => {
            setData(prev => [...prev].filter(x => !checked.has(x.hash)));
            setChecked(new Set());
            console.log(data);
        }
      });
  }

  function setHash(hash) {
    if (!checked.has(hash)) {
        setChecked(_ => new Set([...checked, hash]))
        return;
    }

    setChecked(prev => new Set([...prev].filter(x => x !== hash)));
  }

  return (
    <>
    <NavBar/>
    <div style={{display: 'flex', flexDirection: 'column', minWidth: "100vw"}}>
        <div className="row">
            <button onClick={onDelete}>
                Delete
            </button>
        </div>
        <div className="row">
            <div className="column" style={{minHeight: "100vh"}}>
                {data.map((item, idx) =>
                    (<EmailRow
                        key = {idx}
                        onCheck = { setHash }
                        data= {{sent_mode: props.sent_mode,
                                sender: item.sender,
                                recipient: item.recipient,
                                subject: item.subject,
                                date: item.date,
                                body: item.body,
                                hash: item.hash}} />))
                }
            </div>
            <div className="column">
                <ComposeEmail />
                <Outlet />
            </div>
        </div>
    </div>
    </>
  );
}

export default EmailsPage;
