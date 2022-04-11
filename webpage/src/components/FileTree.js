import $ from 'jquery';
import { useState, useEffect, Component } from 'react';


function count_slashes_in_path(directory_or_file_path) {
  if (!directory_or_file_path) return 0;
  let path = directory_or_file_path;
  var count = 0;
  for (var i = 0; i < path.length; i++) {
    if (path[i] == "/") {
      count++;
    }
  }
  return count;
}


function find_root_dir(dirs) {
  var root_dir = dirs.filter((item) => item == "/");
  if (!root_dir.length) {
    let slash_counts = dirs.map((item) => count_slashes_in_path(item));
    var min_item = slash_counts[0];
    var arg_min = 0;
    slash_counts.forEach((item, index) => {
      if (item < min_item) {
        min_item = item;
        arg_min = index;
      }
    });
    return dirs[arg_min];
  } else {
    return root_dir[0];
  }
}


function find_secondary_level_dirs(dirs) {
  let root_dir = find_root_dir(dirs);
  let root_slashes_count = count_slashes_in_path(root_dir);

  if (root_dir == "/") {
    return dirs.filter((item) => (count_slashes_in_path(item) == 1 && item != "/"));
  } else {
    return dirs.filter((item) => (count_slashes_in_path(item) == root_slashes_count + 1));
  }
}


function find_secondary_level_files(files, dirs) {
  let root_dir = find_root_dir(dirs);
  let root_slashes_count = count_slashes_in_path(root_dir);
  let permited_slash_count = root_dir == "/" ? root_slashes_count : root_slashes_count + 1;
  return files.filter((item) => (item.startsWith(root_dir) && count_slashes_in_path(item) == permited_slash_count));
}


function last_part_of_path(path) {
  if (!path) return "";
  let split_path = path.split("/");
  if (!split_path.length) {
    return "/";
  }
  return split_path[split_path.length - 1].length ? split_path[split_path.length - 1] : "/";
}


function one_level_up(path) {
  if (path.trim() == "/") {
    return "/";
  }

  var parts_of_path = path.split("/");
  parts_of_path.pop();
  var ret = "/" + parts_of_path.join("/");
  return ret;
}

function deleteOnClick(filepath) {
  $.ajax({
    url: `http://localhost:8000/delete_file?${filepath}`,
    method: 'GET',
    crossDomain: true,
    xhrFields: {
        withCredentials: true,
    },
    success: (res) => {window.location.reload()}
  });
}


class FileTree extends Component {

  moveFileOnSubmit(event, old_filepath, new_filepath) {
    event.preventDefault();
    $.ajax({
      url: `http://localhost:8000/move_file`,
      method: 'POST',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({
          'old_absolute_filepath': old_filepath,
          'new_absolute_filepath': new_filepath,
      }),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        if (res["moved_file"]) {
          window.location.reload();
        } else {
          this.setState({move_error: res["error"], show_rename_box: false});
        }
      }
    });
  }

  shareFileOnSubmit(event, filepath, recipient) {
    console.log("filepath: " + filepath);
    console.log("recipient: " + recipient);

    event.preventDefault();
    $.ajax({
      url: `http://localhost:8000/share_file?recipient_and_filepath=${recipient}@@@${filepath}`,
      method: 'GET',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({}),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        console.log("success");
        console.log(res);
      }
    });
  }

  deleteDirOnSubmit(event, dirpath) {
    event.preventDefault();
    $.ajax({
      url: `http://localhost:8000/delete_dir?${dirpath}`,
      method: 'GET',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({}),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        console.log("success");
        console.log(res);
        window.location.reload();
      }
    });
  }

  renameDirOnSubmit(event, old_dirpath, new_dir_name) {
    console.log(old_dirpath);
    console.log(new_dir_name);
    event.preventDefault();
    $.ajax({
      url: `http://localhost:8000/rename_dir`,
      method: 'POST',
      crossDomain: true,
      dataType: 'json',
      data: JSON.stringify({
        old_dir_name: old_dirpath,
        new_dir_name: new_dir_name,
      }),
      xhrFields: {
          withCredentials: true,
      },
      success: (res) => {
        console.log("success");
        console.log(res);
        window.location.reload();
      }
    });
  }

  constructor(props) {
    super(props);

    this.state = {
      rootDir: "",
      secondaryLevelDirs: [],
      show_rename_box: false,
      move_form_data: "",
      move_error: "",
      show_share_box: false,
      share_form_data: "",
      show_rename_dir: false,
      rename_dir_form_data: "",
    };

  }


  render() {

    if (!this.props.filepath) {
      this.state.rootDir = find_root_dir(this.props.dirs);
      this.state.secondaryLevelDirs = find_secondary_level_dirs(this.props.dirs);
      this.state.secondaryLevelFiles =find_secondary_level_files(this.props.files, this.props.dirs);

      console.log("root dir: " + this.state.rootDir + ", second level dirs: " + this.state.secondaryLevelDirs);


      var trees_array = [];
      for (var i = 0; i < this.state.secondaryLevelDirs.length; i++) {
        let second_level_dir = this.state.secondaryLevelDirs[i];

        var dirs_for_tree = [];
        var files_for_tree = [];

        if (this.props.show_full_tree) {
          files_for_tree = this.props.files.filter((item) => item.startsWith(second_level_dir));
          dirs_for_tree = this.props.dirs.filter((item) => item.startsWith(second_level_dir));
        } else {
          dirs_for_tree = [second_level_dir];
        }

        trees_array.push(<FileTree show_full_tree={this.props.show_full_tree} dirs={dirs_for_tree} files={files_for_tree} />);
      }

      for (var i = 0; i < this.state.secondaryLevelFiles.length; i++) {
        trees_array.push(<FileTree filepath={this.state.secondaryLevelFiles[i]} />);
      }

      let back_button = this.props.top_level ? (<p style={{fontWeight: "bold"}} onClick={() => {window.location.href=`/files?${one_level_up(this.state.rootDir).replace("//", "/")}`}}>. .</p>) : (<></>);
      let delete_button = !this.props.top_level ? (<button style={{marginRight: "15px"}} onClick={(e) => this.deleteDirOnSubmit(e, this.state.rootDir)}>Delete directory</button>) : <></>

      let rename_dir_button = !this.props.top_level ? (<button style={{marginRight: "15px"}} onClick={(e) => {this.setState({show_rename_dir: !this.state.show_rename_dir})}}>Rename directory</button>) : <></>
      let rename_dir_display = this.state.show_rename_dir ? "inline" : "none";

      return (
        <div style={{marginLeft: "5px"}}>
          <div style={{paddingLeft: "10px"}}>
            {back_button}
            <div className="file_row">
            {delete_button}
            {rename_dir_button}

            <form style={{display: rename_dir_display, marginLeft: "15px"}} onSubmit={(e) => this.renameDirOnSubmit(e, this.state.rootDir, this.state.rename_dir_form_data)}>
              <label>New directory name:</label>
              <input type="text"
              name="rename_dir_input"
              style={{display: "inline", width: "300px", height: "20px", marginLeft: "15px", marginRight: "15px"}}
              value={this.state.rename_dir_form_data}
              onChange={(e) => this.setState({rename_dir_form_data: e.target.value})}/>
              <input type="submit" value="Rename directory"/>
            </form>

            <p style={{fontWeight: "bold"}} onClick={() => {window.location.href=`/files?${this.state.rootDir}`}}>{last_part_of_path(this.state.rootDir)}</p>


            </div>
            {trees_array}
          </div>
        </div>
      );

    } else {
      let move_form_display = this.state.show_rename_box ? "inline" : "none";
      let share_form_display = this.state.show_share_box ? "inline" : "none";
      let error_message = this.state.move_error ? <a style={{color: "red", marginLeft: "15px"}}>{this.state.move_error}</a> : <></>

      return (
        <div style={{marginLeft: "5px"}}>
          <div style={{paddingLeft: "10px"}}>
            <div style={{display: "inline"}}>
              <button style={{marginRight: "15px"}} onClick={() => deleteOnClick(this.props.filepath)}>Delete file</button>
              <button style={{marginRight: "15px"}} onClick={() => {this.setState({show_rename_box: !this.state.show_rename_box, show_share_box: false}); this.setState({move_error: ""})}}>Move file</button>
              <button style={{marginRight: "15px"}} onClick={() => {this.setState({show_share_box: !this.state.show_share_box, show_rename_box: false})}}>Share file</button>

              <a target="_blank" href={`http://localhost:8000/file?name=${this.props.filepath}`}>{last_part_of_path(this.props.filepath)}</a>

              <form style={{display: move_form_display, marginLeft: "15px"}} onSubmit={(e) => this.moveFileOnSubmit(e, this.props.filepath, this.state.move_form_data)}>
                <label>New file path:</label>
                <input type="text"
                       name="new_filepath_input"
                       style={{display: "inline", width: "300px", height: "20px", marginLeft: "15px", marginRight: "15px"}}
                       value={this.state.move_form_data}
                       onChange={(e) => this.setState({move_form_data: e.target.value})}/>
                <input type="submit" value="Move file"/>
              </form>
              {error_message}

              <form style={{display: share_form_display, marginLeft: "15px"}} onSubmit={(e) => this.shareFileOnSubmit(e, this.props.filepath, this.state.share_form_data)}>
                <label>Username to share with:</label>
                <input type="text"
                       name="share_input"
                       style={{display: "inline", width: "300px", height: "20px", marginLeft: "15px", marginRight: "15px"}}
                       value={this.state.share_form_data}
                       onChange={(e) => this.setState({share_form_data: e.target.value})}/>
                <input type="submit" value="Share file"/>
              </form>
            </div>
          </div>
        </div>
      );
    }

  }
}


export default FileTree;
