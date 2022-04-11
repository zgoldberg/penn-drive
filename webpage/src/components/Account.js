import $ from 'jquery';
import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import '../Login.css';
import ChangePassword from './ChangePassword'
import NavBar from './NavBar';

function Account(props) {
  const nav = useNavigate();
  const [password, setPassword] = useState("");
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
      }
    });
  }, []);


  let div_style = {
    textAlign: "center",
  };

  return (
    <>
    <NavBar />
    <div style={div_style}>
      <h1>{`Welcome, ${username}`}</h1>
      <ChangePassword username={username}/>
    </div>
    </>
  );
}

export default Account;
