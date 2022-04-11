import $ from 'jquery';
import { useState, useEffect } from 'react';

function NavBar() {

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
    overflow: "hidden",
    backgroundColor: "grey",
    float: "left",
    color: "white",
    textAlign: "center",
    padding: "15px",
    fontSize: "20px",
    width: "100%",
    margin: "0",
    marginBottom: "15px"
  };

  let a_style = {
    float: "left",
    color: "white",
    textAlign: "center",
    padding: "10px",
    fontSize: "15px"
  }

  let pc_style = {
    ...a_style,
    textDecoration: "none",
    fontWeight: "bold",
  }

  return (
    <div style={div_style}>
      <a style={pc_style} href="/menu">PennCloud</a>
      <a style={a_style} href="/emails">Inbox</a>
      <a style={a_style} href="/sent_emails">Sent Mail</a>
      <a style={a_style} href="/files">Files</a>
      <a style={a_style} href="/account">Account</a>
      <a style={a_style} href="/admin">Admin Console</a>
      <a style={a_style} href="/admin_data">Admin Data</a>
      <a style={a_style}>{username}</a>
    </div>
  );
}

export default NavBar;
