import $ from 'jquery';
import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import '../Login.css';

function Register(props) {
  const nav = useNavigate();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");

  const handleRegister = () => {
      $.ajax({
        url: `http://localhost:8000/account`,
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
            if (res.auth) {
                nav("../menu");
            } else {
                console.log('unauthorized');
            }
        }
      });
  };

  return (
    <div>
        <div className="content container">
            <div className="container">
                <div>Welcome, new user! Please create an account below</div>
            </div>
            <label><b>Username</b></label>
            <input type="text" placeholder="Enter Username"
                value={username}
                onChange={(e) => setUsername(e.target.value)} />
            <label><b>Password</b></label>
            <input onChange={(e) => setPassword(e.target.value)} type="password" placeholder="Enter Password" value={password} />
            <button className="login_btn" onClick={handleRegister}>Register</button>
        </div>
    </div>
  );
}

export default Register;
