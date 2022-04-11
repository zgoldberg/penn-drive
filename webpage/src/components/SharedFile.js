import $ from 'jquery';
import { useState, useEffect } from 'react';

function SharedFile(props) {
  const [showShare, setShowShare] = useState(false);
  const [shareWithAddress, setShareWithAddress] = useState("");

  return (
    <>
      <p>
        {props.sender}:&nbsp;&nbsp;
        <a href={`http://localhost:8000/shared_file?sender_and_filename=${props.sender}@@@${props.filename}`}>
          {props.filename}
        </a>
      </p>
    </>
  );

}

export default SharedFile;
