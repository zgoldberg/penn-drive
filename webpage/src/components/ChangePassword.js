import $ from 'jquery';
import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import '../Login.css';

function ChangePassword(props) {
  const nav = useNavigate();
  const [password, setPassword] = useState("");
  var [username, setUsername] = useState("");
  var [responseMessage, setResponseMessage] = useState("");


  if (props.username) {
    username = props.username;
  }

  let username_box = props.username ? <></> : (
    <>
      <label><b>Username</b></label>
      <input type="text" placeholder="Enter Username"
          value={username}
          onChange={(e) => setUsername(e.target.value)} />
    </>
  );

  let response_message = responseMessage == "" ? <></> : (
    <>
      <p>{responseMessage}</p>
    </>
  );


  const handleChangePassword = () => {
      $.ajax({
        url: `http://localhost:8000/change_password`,
        method: 'POST',
        crossDomain: true,
        xhrFields: {
            withCredentials: true,
        },
        dataType: 'json',
        data: JSON.stringify({
            username: username,
            password: password,
        }),
        success: (res) => {
            console.log(res);
            if (res["response_code"] == 0) {
              setResponseMessage("Password changed!");
            } else if (res["response_code"] == 1) {
              setResponseMessage("No such user");
            } else if (res["response_code"] == 2) {
              setResponseMessage("New password matches old password");
            } else if (res["response_code"] == 3) {
              setResponseMessage("Error changing password");
            }
        }
      });
  };

  let div_style = {
    textAlign: "center",
  };

  return (
    <div>
        <div className="content container">
            <h2>Change Password</h2>
            {username_box}
            <label><b>Password</b></label>
            <input onChange={(e) => setPassword(e.target.value)} type="password" placeholder="Enter Password" value={password} />
            <button className="login_btn" onClick={handleChangePassword}>Change Password</button>
            {response_message}
        </div>
    </div>
  );
}

export default ChangePassword;
