import $ from 'jquery';
import { useState, useEffect } from 'react';
import UploadFile from './UploadFile';
import FileTree from './FileTree';
import SharedFile from './SharedFile';
import NavBar from './NavBar';


function ViewFiles() {

  const [allFiles, setAllFiles] = useState([]);
  const [allDirs, setAllDirs] = useState([]);
  const [showFullTree, setShowFullTree] = useState();
  // const [sharedFiles, setSharedFiles] = useState([["zachg@localhost", "beach.jpeg"]]);
  const [sharedFiles, setSharedFiles] = useState([]);

  function parse_shared_files(shared_files_metadata) {
    let senders_and_files_strings = shared_files_metadata.split(";");
    var senders_and_files_objects = senders_and_files_strings.map(i => [i.split('@@@')[0], i.split('@@@')[1]]);
    senders_and_files_objects = senders_and_files_objects.filter(i => i[0] != "");
    setSharedFiles(senders_and_files_objects);
    console.log(senders_and_files_objects);
  }

  // Only gets called when page is first loaded
  useEffect(() => {
      $.ajax({
        url: `http://localhost:8000/files?${window.location.href.split("?").length > 1 ? window.location.href.split("?")[1] : "/"}`,
        method: 'GET',
        xhrFields: {
            withCredentials: true,
        },
        crossDomain: true,
        dataType: 'json',
        success: (res) => {
            if (res.all_files) {
              let all_files = res.all_files.split(";");
              all_files = all_files.filter(element => element != "");
              let all_dirs = res.all_dirs.split(";");
              all_dirs = all_dirs.filter(element => element != "");
              setAllFiles(all_files);
              setAllDirs(all_dirs);
            }

        }
      });
      setShowFullTree(false);

      $.ajax({
        url: `http://localhost:8000/shared_files`,
        method: 'GET',
        xhrFields: {
            withCredentials: true,
        },
        crossDomain: true,
        dataType: 'json',
        success: (res) => {
            console.log("RES");
            console.log(res);
            parse_shared_files(res.shared_files_metadata);
        }
      });

  }, []);

  console.log(sharedFiles);

  return (
    <>
      <NavBar/>
        <div style={{padding: "10px"}}>
          <UploadFile/>
          <h1>Your files:</h1>
          <button onClick={() => setShowFullTree(!showFullTree)}>Toggle show full file tree</button>
          <FileTree show_full_tree={showFullTree} dirs={allDirs} files={allFiles} top_level={true}/>
          {sharedFiles.length ? <h1>Shared files:</h1> : <></>}
          {sharedFiles.map(sender_and_file => <SharedFile sender={sender_and_file[0]} filename={sender_and_file[1]}/>)}
      </div>
    </>
  );

}


export default ViewFiles;
