import $ from 'jquery';
import '../App.css';
import { Link, useParams, useLocation, useNavigate } from 'react-router-dom';
import { useState, useEffect } from 'react';


export default function EmailView(props) {
  let { hash_link } = useParams();
  let hash = decodeURI(hash_link)
  const nav = useNavigate();

  const location = useLocation();
  const state = location.state;


  const [username, setUsername] = useState("");

  useEffect(() => {
    $.ajax({
      url: `http://localhost:8000/get_username`,
      method: 'GET',
      xhrFields: {
          withCredentials: true,
      },
      crossDomain: true,
      dataType: 'json',
      success: (res) => {
          setUsername(res["username"]);
          console.log(res["username"]);
      }
    });
  }, []);


  let { sender, subject, date, body } = location.state;


  var new_body = "";
  let content_line = "Content-Transfer-Encoding: 7bit\r\n";
  if (subject.includes("Fwd:") && body.includes(content_line)) {
    let index = body.indexOf(content_line);
    new_body = body.substr(index + content_line.length, body.length - index);
    let weird_line = "--------------";
    let index_2 = new_body.indexOf(weird_line);
    new_body = new_body.substr(0, index_2);
  } else {
    new_body = body;
  }


  const [replyText, setReplyText] = useState("");
  const [forwardAddress, setForwardAddress] = useState("");
  const [forwardMessage, setForwardMessage] = useState("");

  function handleReply(event) {
    event.preventDefault();
    let reply_subject = `RE: ${subject}`;
    let reply_recipient = sender.replace("<", "").replace(">", "").trim();
    let reply_body = replyText;
    reply_body += '\r\n\r\n';

    let split_body = body.split("\r\n");

    for (var i = 0; i < split_body.length; i++) {
      reply_body += "> " + split_body[i] + "\r\n";
    }

    console.log("subject: " + reply_subject);
    console.log("recipient: " + reply_recipient);
    console.log("body: " + reply_body);

    $.ajax({
      url: `http://localhost:8000/compose`,
      method: 'POST',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({
          'recipient': reply_recipient,
          'subject': reply_subject,
          'body': reply_body,
      }),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        window.location.reload();
      }
    });

  }


  function handleForward(event) {
    event.preventDefault();
    let forward_subject = `Fwd: ${subject}`;
    let forward_recipient = forwardAddress;
    let forward_body = forwardMessage;
    forward_body += '\r\n\r\n-------- Forwarded Message --------\r\n';
    forward_body += `Subject: ${subject}`;
    forward_body += `Date: ${date}`;
    forward_body += `From: ${sender.replace("<", "").replace(">", "").trim()}`;
    forward_body += `To: ${username}`;
    forward_body += '\r\n\r\n';
    forward_body += new_body;

    $.ajax({
      url: `http://localhost:8000/compose`,
      method: 'POST',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({
          'recipient': forward_recipient,
          'subject': forward_subject,
          'body': forward_body,
      }),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        window.location.reload();
      }
    });

  }


  return (
    <div className="card column" style={{width: "80%", marginLeft: "50px", marginRight: "50px"}}>
        <h3>{sender}</h3>
        <h3>{subject}</h3>
        <h3>{date}</h3>
        <h3 style={{whiteSpace: "pre-wrap"}}>{new_body}</h3>
        <form onSubmit={handleReply}>
          <label>Reply to email:</label>
          <br/>
          <textarea value={replyText} onChange={(e) => setReplyText(e.target.value)} />
          <input type="submit" value="Reply"/>
        </form>

        <br/>

        <form onSubmit={handleForward}>
          <label>Forward email:</label>
          <br/><label>Forward address:</label><br/>
          <textarea value={forwardAddress} onChange={(e) => setForwardAddress(e.target.value)} />
          <br/><label>Forward message:</label><br/>
          <textarea value={forwardMessage} onChange={(e) => setForwardMessage(e.target.value)} />
          <input type="submit" value="Forward"/>
        </form>

        <a href="/emails">Back to inbox</a>
    </div>
  );
}

// export { EmailView };
