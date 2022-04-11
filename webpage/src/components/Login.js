import $ from 'jquery';
import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import '../Login.css';

function Login(props) {
  const nav = useNavigate();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");

  const handleLogin = () => {
      $.ajax({
        url: `http://localhost:8000/signin`,
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
                nav("menu");
            } else {
                console.log('unauthorized');
            }
        }
      });
  };

  return (
    <div>
        <div className="content container">
            <label><b>Username</b></label>
            <input type="text" placeholder="Enter Username"
                value={username}
                onChange={(e) => setUsername(e.target.value)} />
            <label><b>Password</b></label>
            <input onChange={(e) => setPassword(e.target.value)} type="password" placeholder="Enter Password" value={password} />
            <button className="login_btn" onClick={handleLogin}>Login</button>

            <div>
                Forgot <Link to="/change_password">password?</Link>
                <div> Or </div>
                <Link to="/new_account">create a new account</Link>
            </div>
        </div>
    </div>
  );
}

export default Login;
