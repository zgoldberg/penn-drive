import '../App.css';
import { useState } from 'react';
import { Link } from 'react-router-dom';

function EmailRow(props) {
  const [checked, setChecked] = useState(false);
  let {sent_mode, sender, recipient, subject, date, body, hash } = props.data;
  let onCheck = props.onCheck;
  let hash_link = encodeURI(hash).replace(/\//g, '-');

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

  //Content-Type: text/plain; charset=utf-8; format=flowed

  return (
    <div className="row">
        <form>
            <input type="checkbox"
                checked={checked}
                onChange={() => {
                    onCheck(hash);
                    setChecked(!checked);
                }}
            />
        </form>
        <Link
                className="column card"
                state={{'sender': sender,
                        'recipient': recipient,
                        'subject': subject,
                        'date': date,
                        'body': body}}
                to={`/${sent_mode ? "sent_emails" : "emails"}/${hash_link}`}>
            <div className="row">
                <div className="container column">
                    <b><div>{subject}</div></b>
                    <div>To: {recipient}</div>
                    <div>From: {sender}</div>
                </div>
                <div className="container column">
                    {date}
                </div>
            </div>
            <div className="row">
                <div style={{whiteSpace: "pre-wrap"}} className="container">{new_body}</div>
            </div>
        </Link>
    </div>
  );
}

export default EmailRow;
