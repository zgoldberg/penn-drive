import $ from 'jquery';
import { useState } from 'react';

function UploadFile() {

  const [dirName, setDirName] = useState("");

    function filechange(event) {
      event.preventDefault();
      console.log(event.target.files[0]);

      let file = event.target.files[0];

      let form_data = new FormData();
      form_data.append('image', file);

      $.ajax({
        url: `http://localhost:8000/upload_file?${window.location.href.split("?").length > 1 ? window.location.href.split("?")[1] : "/"}`,
        crossDomain: true,
        contentType: false,
        processData: false,
        type: 'POST',
        data: form_data,
        xhrFields: {
            withCredentials: true,
        },
        success: (res) => {
          window.location.reload()
        },
      });
    }

    function handleDirSubmit(event) {
      event.preventDefault();

      var new_dir_name = window.location.href.split("?").length > 1 ? window.location.href.split("?")[1] : "/";
      new_dir_name += new_dir_name[new_dir_name.length - 1] == "/" ? dirName : "/" + dirName;

      $.ajax({
        url: `http://localhost:8000/create_dir?${new_dir_name}`,
        method: 'GET',
        crossDomain: true,
        dataType: 'json',
        xhrFields: {
            withCredentials: true,
        },
        success: (res) => {
          console.log(43);
          setDirName("");
          window.location.reload();
        }
      });
    }

    return (
      <div>
      <form>
        <label>
        Choose file:
        </label>
        <input type="file" onChange={(e) => filechange(e)}/>
        </form>
        <form onSubmit={handleDirSubmit}>
          <label>Create directory:</label>
          <textarea value={dirName} onChange={(e) => setDirName(e.target.value)} />
          <input type="submit" value="Submit"/>
        </form>
      </div>
    );
}


export default UploadFile;
