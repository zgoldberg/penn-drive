import $ from 'jquery';
import { useState, useEffect } from 'react';
import email_logo from '../images/email_clipart.png';
import file_logo from '../images/file_clipart.jpg';
import admin_logo from '../images/user_icon.png';

function Menu() {

  const { innerWidth: width, innerHeight: height } = window;

  let outer_div_style = {
    margin: "25px",
    width: "100%",
    height: height * 0.66 + "px",
    position: "relative",
    textAlign: "center"
  };

  let inner_div_style = {
    position: "absolute",
    top: "0",
    bottom: "0",
    left: "0",
    right: "0",
    margin: "auto",
    height: "1px",
  };

  let inner_inner_div_style = {
    display: "inline-block",
  };

  let a_style = {
    display: "inline-block",
    marginRight: "40px",
    marginLeft: "40px",
  }

  let image_style = {
    width: "100px",
    height: "auto",
  }

  return (
    <div style={outer_div_style}>
      <h1>PennCloud</h1>
      <div style={inner_div_style}>

        <div style={inner_inner_div_style}>
          <a style={a_style} href="/emails">
            <img style={image_style} src={email_logo} alt="Email logo" />
            <br/>
            Emails
          </a>
        </div>

        <div style={inner_inner_div_style}>
          <a style={a_style} href="/files">
            <img style={image_style} src={file_logo} alt="File logo" />
            <br/>
            Files
          </a>
        </div>

        <div style={inner_inner_div_style}>
          <a style={a_style} href="/admin">
            <img style={image_style} src={admin_logo} alt="Admin logo" />
            <br/>
            Admin
          </a>
        </div>


      </div>
    </div>
  );

}

export default Menu;
