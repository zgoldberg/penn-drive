import $ from 'jquery';
import { useState } from 'react';

function ComposeForm() {
  const [recipient, setRecipient] = useState("");
  const [subject, setSubject] = useState("");
  const [body, setBody] = useState("");

  function handleSubmit(event) {
    event.preventDefault();
    $.ajax({
      url: `http://localhost:8000/compose`,
      method: 'POST',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({
          'recipient': recipient,
          'subject': subject,
          'body': body,
      }),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        setRecipient("");
        setSubject("");
        setBody("");
      }
    });
  }

  return (
    <form className="card column" style={{width: "80%", marginLeft: "50px", marginRight: "50px"}} onSubmit={handleSubmit}>
      <label>
        Recipient:
        <textarea
            value={recipient}
            onChange={(e) => setRecipient(e.target.value)} />
      </label>
      <label>
        Subject:
        <textarea
            value={subject}
            onChange={(e) => setSubject(e.target.value)} />
      </label>
      <label>
        Message:
        <textarea
            value={body}
            onChange={(e) => setBody(e.target.value)} />
      </label>
      <input type="submit" value="Submit" />
    </form>
  );
}

export default ComposeForm;
