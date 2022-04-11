#include "files.h"
#include "common_mail_files.h"


using namespace std;
using namespace rapidjson;



bool init_files_metadata(string username, KeyValueClient &client) {
  GetReply get_response = client.get_call(username, "/,files");
  if (get_response.response() != kvstore::ResponseCode::MISSING_KEY) {
    return false;
  }

  PutReply put_response = client.put_call(username, "/,files", ";");
  put_response = client.put_call(username, "/,dirs", ";");
  put_response = client.put_call(username, "$$$shared_files", ";");
  return true;
}


bool share_file(string sender, string recipient, string absolute_file_path, KeyValueClient &client) {

  init_files_metadata(recipient, client);
  string metadata_column_name = "$$$shared_files";

  if (absolute_file_path.front() != '/') {
    absolute_file_path.insert(0, 1, '/');
  }

  MasterBackendClient master_client(
      grpc::CreateChannel(
          "localhost:5000",
          grpc::InsecureChannelCredentials()));

  string channel_recipient = master_client.which_node_call(recipient);
  KeyValueClient other_client = KeyValueClient(
      grpc::CreateChannel(
          channel_recipient,
          grpc::InsecureChannelCredentials()));


  string to_append(sender);
  to_append.append("@@@");
  to_append.append(absolute_file_path);


  GetReply get_reply = other_client.get_call(recipient, metadata_column_name);
  string metadata_to_check = get_reply.value();
  auto metadatas = separate_string(metadata_to_check, ";");
  if (string_in_vector(to_append, &metadatas)) {
    return true;
  }

  append_to_metadata(recipient, metadata_column_name, to_append, other_client);

  return true;
}


string get_shared_files_metadata(string username, KeyValueClient &client) {
  string metadata_column_name = "$$$shared_files";
  GetReply get_response = client.get_call(username, metadata_column_name);
  if (get_response.error().size()) {
    fprintf(stderr, "1st get failed\n");
    return "";
  } else if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
    return "";
  } else {
    return get_response.value();
  }
}


int write_file(string username, string filename, string data, KeyValueClient &client) {
  init_files_metadata(username, client);

  if (filename.front() != '/') {
    filename.insert(0, 1, '/');
  }

  // update directory-level metadata
  build_out_directory_path(username, filename, client);

  delete_file(username, filename, client); // to overwrite file

  string metadata_column_name = one_level_up(filename);
  metadata_column_name.append(",files");

  GetReply get_response = client.get_call(username, metadata_column_name);
  if (get_response.error().size()) {
    fprintf(stderr, "1st get failed\n");
    return 0;
  }

  bool cput_loop_response = append_to_metadata(username, metadata_column_name, filename, client);


  // update file-level metadata and write file data
  PutReply file_metadata_put_response = client.put_call(username, filename, ";");

  // string filename_shared_with_col_name(filename);
  // filename_shared_with_col_name.append(",shared_with");
  // PutReply shared_with_put_response = client.put_call(username, filename_shared_with_col_name, ";");


  int num_full_chunks = 1;
  int last_chunk_size = 0;

  if (data.length() > CHUNK_SIZE_BYTES) {
    num_full_chunks = data.length() / CHUNK_SIZE_BYTES;
    last_chunk_size = data.length() % CHUNK_SIZE_BYTES;
  }

  int i;
  for (i = 0; i < num_full_chunks; i++) {
    string chunk_col_name(filename);
    chunk_col_name.append(",");
    chunk_col_name.append(to_string(i));
    append_to_metadata(username, filename, chunk_col_name, client);

    int start_byte = i * CHUNK_SIZE_BYTES;
    string chunk_data = "";
    chunk_data.append(data.substr(start_byte, CHUNK_SIZE_BYTES));
    PutReply chunk_put_response = client.put_call(username, chunk_col_name, chunk_data);
  }

  if (last_chunk_size) {
    string chunk_col_name(filename);
    chunk_col_name.append(",");
    chunk_col_name.append(to_string(i));
    append_to_metadata(username, filename, chunk_col_name, client);
    int start_byte = i * CHUNK_SIZE_BYTES;
    string chunk_data = data.substr(start_byte, CHUNK_SIZE_BYTES);
    PutReply chunk_put_response = client.put_call(username, chunk_col_name, chunk_data);
  }

  return cput_loop_response;
}


string get_files(string username, KeyValueClient &client) {
  bool init_res = init_files_metadata(username, client);

  string metadata_column_name("/,files");
  GetReply get_response = client.get_call(username, metadata_column_name);
  if (get_response.error().size()) {
    fprintf(stderr, "1st get failed\n");
    return 0;
  }
  string metadata = get_response.value();
  return metadata;
}


string get_file_data(string username, string absolute_file_path, KeyValueClient &client) {
  init_files_metadata(username, client);

  MasterBackendClient master_client(
      grpc::CreateChannel(
          "localhost:5000",
          grpc::InsecureChannelCredentials()));

  string channel_recipient = master_client.which_node_call(username);
  client = KeyValueClient(
      grpc::CreateChannel(
          channel_recipient,
          grpc::InsecureChannelCredentials()));

  string filename(absolute_file_path);
  if (filename.front() != '/') {
    filename.insert(0, 1, '/');
  }


  GetReply get_response = client.get_call(username, filename);
  if (get_response.error().size()) {
    fprintf(stderr, "1st get failed\n");
    return "";
  } else if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
    return "NO SUCH FILE";
  }

  string file_metadata = get_response.value();
  auto file_chunk_cols = separate_string(file_metadata, DELIMITER_STRING);
  string file_data("");

  for (unsigned long i = 0; i < file_chunk_cols.size(); i++) {
    string chunk_col_name = file_chunk_cols.at(i);
    GetReply chunk_get_response = client.get_call(username, chunk_col_name);
    if (get_response.error().size()) {
      fprintf(stderr, "110 get failed\n");
      return 0;
    }
    string chunk_data = chunk_get_response.value();

    file_data.append(chunk_data);
  }

  return file_data;
}


string get_files_in_dir(string username, string directory_path, KeyValueClient &client) {
  init_files_metadata(username, client);

  if (directory_path.back() == '/' && directory_path.front() != '/') {
    directory_path.pop_back();
  }

  string directory_files_column(directory_path);
  directory_files_column.append(",files");

  GetReply get_response = client.get_call(username, directory_files_column);
  string files_metadata = get_response.value();
  return files_metadata;
}


string get_dirs_in_dir(string username, string directory_path, KeyValueClient &client) {
  init_files_metadata(username, client);

  if (directory_path.back() == '/' && directory_path.front() != '/') {
    directory_path.pop_back();
  }

  string directory_dirs_column(directory_path);
  directory_dirs_column.append(",dirs");

  GetReply get_response = client.get_call(username, directory_dirs_column);
  string files_metadata = get_response.value();
  return files_metadata;
}


pair<string, string> get_all_files_json(string username, string root, Document::AllocatorType& allocator, KeyValueClient &client) {
  init_files_metadata(username, client);

  vector<string> absolute_file_paths;
  vector<string> absolute_dir_paths;

  vector<string> dirs_stack;
  dirs_stack.push_back(root);
  absolute_dir_paths.push_back(root);

  while (dirs_stack.size()) {
    string current_dir = dirs_stack.back();
    dirs_stack.pop_back();

    string files_metadata_column(current_dir);
    files_metadata_column.append(",files");
    string dirs_metadata_column(current_dir);
    dirs_metadata_column.append(",dirs");

    GetReply get_response = client.get_call(username, files_metadata_column);
    if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
      cout << "1 ERROR no such kv pair (" << username << ", " << files_metadata_column << ")\n";
      break;
    }
    string current_dir_files_metadata = get_response.value();
    vector<string> files_in_current_dir = separate_string(current_dir_files_metadata, ";");

    for (unsigned long i = 0; i < files_in_current_dir.size(); i++) {
      absolute_file_paths.push_back(files_in_current_dir.at(i));
    }


    get_response = client.get_call(username, dirs_metadata_column);
    if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
      cout << "2 ERROR no such kv pair (" << username << ", " << dirs_metadata_column << ")\n";
      break;
    }
    string current_dir_dirs_metadata = get_response.value();
    vector<string> dirs_in_current_dir = separate_string(current_dir_dirs_metadata, ";");

    for (unsigned long i = 0; i < dirs_in_current_dir.size(); i++) {
      absolute_dir_paths.push_back(dirs_in_current_dir.at(i));
      dirs_stack.push_back(dirs_in_current_dir.at(i));
    }
  }

  string all_files_string(";");
  string all_dirs_string(";");

  for (const auto file_path: absolute_file_paths) {
    all_files_string.append(file_path);
    all_files_string.append(";");
  }

  for (const auto dir_path: absolute_dir_paths) {
    all_dirs_string.append(dir_path);
    all_dirs_string.append(";");
  }

  pair<string, string> files_and_dirs_strings(all_files_string, all_dirs_string);
  return files_and_dirs_strings;
}



string one_level_up(string path) {
  if (path.front() != '/') {
    path.insert(0, 1, '/');
  }

  auto directories_in_path = separate_string(path, "/");

  if (directories_in_path.size() < 2) {
    return "/";
  }

  directories_in_path.pop_back(); // remove filename

  string new_path("");
  for (auto const part : directories_in_path) {
    new_path.append("/");
    new_path.append(part);
  }

  return new_path;
}


void create_dir(string username, string directory_path, KeyValueClient &client) {
  string files_column_name(directory_path);
  string dirs_column_name(directory_path);
  files_column_name.append(",files");
  dirs_column_name.append(",dirs");

  // create metadata cells
  PutReply put_response = client.put_call(username, files_column_name, ";");
  put_response = client.put_call(username, dirs_column_name, ";");

  // link to previous directory
  string parent_directory_dirs_metadata_column = one_level_up(directory_path);
  parent_directory_dirs_metadata_column.append(",dirs");
  append_to_metadata(username, parent_directory_dirs_metadata_column, directory_path, client);
}


bool build_out_directory_path(string username, string absolute_file_path, KeyValueClient &client) {
  init_files_metadata(username, client);

  if (absolute_file_path.front() != '/') {
    absolute_file_path.insert(0, 1, '/');
  }

  bool added_dir = false;

  auto directories_in_path = separate_string(absolute_file_path, "/");
  directories_in_path.pop_back(); // remove filename

  for (unsigned long i = 0; i < directories_in_path.size(); i++) {
    // build string up to ith directory
    string path_up_to_ith_directory("");
    for (unsigned long j = 0; j <= i; j++) {
      path_up_to_ith_directory.append("/");
      path_up_to_ith_directory.append(directories_in_path.at(j));
    }

    string files_column_name(path_up_to_ith_directory);
    files_column_name.append(",files");
    GetReply get_response_files = client.get_call(username, files_column_name);
    if (get_response_files.response() == kvstore::ResponseCode::MISSING_KEY) {
      cout << "no column: " << files_column_name << "\n";
      cout << "creating directory: " << path_up_to_ith_directory << "\n";
      create_dir(username, path_up_to_ith_directory, client);
    }

  }

  return added_dir;

}


bool delete_dir(string username, string dir_name, KeyValueClient &client) {

  dir_name = separate_string(dir_name, "&")[0];
  cout << "DIR NAME: " << dir_name << endl;

  Document outDoc;
  outDoc.SetObject();
  Document::AllocatorType& allocator = outDoc.GetAllocator();
  auto files_and_dirs_strings_pair = get_all_files_json(username, dir_name, allocator, client);
  string all_files_string = files_and_dirs_strings_pair.first;
  string all_dirs_string = files_and_dirs_strings_pair.second;

  cout << "ALL FILES: " << all_files_string << endl;
  cout << "ALL DIRS: " << all_dirs_string << endl;

  auto all_files_vec = separate_string(all_files_string, ";");
  for (unsigned long i = 0; i < all_files_vec.size(); i++) {
    delete_file(username, all_files_vec[i], client);
  }

  auto all_dirs_vec = separate_string(all_dirs_string, ";");
  for (unsigned long i = 0; i < all_dirs_vec.size(); i++) {
    string dir_dirs_md(all_dirs_vec[i]);
    dir_dirs_md.append(",dirs");
    string dir_files_md(all_dirs_vec[i]);
    dir_files_md.append(",files");

    DeleteReply delete_reply = client.delete_call(username, dir_dirs_md);
    delete_reply = client.delete_call(username, dir_files_md);
  }

  string dir_directory_metadata_col = one_level_up(dir_name);
  dir_directory_metadata_col.append(",dirs");

  GetReply get_reply = client.get_call(username, dir_directory_metadata_col);
  if (get_reply.response() == kvstore::ResponseCode::MISSING_KEY) {
    cout << "CANT DELETE 397" << endl;
    return false;
  }

  string metadata = get_reply.value();
  metadata = delete_hash_from_metadata_string(metadata, dir_name);
  cput_loop(username, dir_directory_metadata_col, metadata, client);

  return true;
}


bool delete_file(string username, string absolute_file_path, KeyValueClient &client) {
  GetReply get_response_file_metadata = client.get_call(username, absolute_file_path);

  if (get_response_file_metadata.response() == kvstore::ResponseCode::MISSING_KEY) {
    return false;
  }

  string file_metadata = get_response_file_metadata.value();
  auto file_chunk_cols = separate_string(file_metadata, DELIMITER_STRING);
  for (unsigned long i = 0; i < file_chunk_cols.size(); i++) {
    string chunk_col_name = file_chunk_cols.at(i);
    client.delete_call(username, chunk_col_name);
  }

  client.delete_call(username, absolute_file_path);

  string file_directory_metadata_col = one_level_up(absolute_file_path);
  file_directory_metadata_col.append(",files");
  GetReply get_response_file_dir_metadata = client.get_call(username, file_directory_metadata_col);
  string file_directory_metadata = get_response_file_dir_metadata.value();
  string new_file_directory_metadata = delete_hash_from_metadata_string(file_directory_metadata, absolute_file_path);

  client.put_call(username, file_directory_metadata_col, new_file_directory_metadata);

  // trying to unshare files but not working
  // string filename_shared_with_col_name(absolute_file_path);
  // filename_shared_with_col_name.append(",shared_with");
  // GetReply get_response_shared_with_metadata = client.get_call(username, filename_shared_with_col_name);
  // if (get_response_file_metadata.response() == kvstore::ResponseCode::MISSING_KEY) {
  //   cout << "UUUUUUUHHHH" << endl;
  //   return true;
  // }
  // string shared_with_metadata = get_response_shared_with_metadata.value();
  // auto shared_with_usernames = separate_string(shared_with_metadata, ";");
  //
  // string shared_with_metadata_in_others(username);
  // shared_with_metadata_in_others.append("@@@");
  // shared_with_metadata_in_others.append(absolute_file_path);
  //
  // MasterBackendClient master_client(
  //     grpc::CreateChannel(
  //         "localhost:5000",
  //         grpc::InsecureChannelCredentials()));
  //
  // for (unsigned long i = 0; i < shared_with_usernames.size(); i++) {
  //   string channel_recipient = master_client.which_node_call(shared_with_usernames[i]);
  //   KeyValueClient other_client = KeyValueClient(
  //       grpc::CreateChannel(
  //           channel_recipient,
  //           grpc::InsecureChannelCredentials()));
  //
  //   GetReply other_shared_metadata_get_response = client.get_call(shared_with_usernames[i], "$$$shared_files");
  //   if (other_shared_metadata_get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
  //     cout << "426 continue" << endl;
  //     continue;
  //   }
  //   string metadata_other = other_shared_metadata_get_response.value();
  //   string new_metadata_other = delete_hash_from_metadata_string(metadata_other, shared_with_metadata_in_others);
  //
  //   cout << "NEW METADATA OTHER: " << new_metadata_other << endl;
  //
  //   cput_loop(shared_with_usernames[i], "$$$shared_files", new_metadata_other, other_client);
  // }

  return true;
}


bool file_exists(string username, string absolute_file_path, KeyValueClient &client) {
  init_files_metadata(username, client);

  string filename(absolute_file_path);
  if (filename.front() != '/') {
    filename.insert(0, 1, '/');
  }

  GetReply get_response = client.get_call(username, filename);

  if (get_response.error().size()) {
    return false;
  } else if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
    return false;
  }

  return true;
}


int move_file(string username, string old_absolute_filepath, string new_absolute_filepath, KeyValueClient &client) {

  if (!file_exists(username, old_absolute_filepath, client)) {
    return 1;
  } else if (file_exists(username, new_absolute_filepath, client)) {
    return 2;
  }

  string data = get_file_data(username, old_absolute_filepath, client);
  delete_file(username, old_absolute_filepath, client);
  write_file(username, new_absolute_filepath, data, client);
  return 0;
}


int count_slashes_in_path(string path) {
  if (path == "/") {
    return 0;
  }
  int count = 0;
  for (unsigned long i = 0; i < path.length(); i++) {
      if (path[i] == '/') count ++;
  }
  return count;
}


string replace_in_path(string path, string to_replace, string replace_with) {
  cout << 543 << endl;
  int start_of_to_replace = path.find(to_replace);
  if (start_of_to_replace == -1) return path;
  cout << "S" << endl;
  string new_path = path.replace(start_of_to_replace, replace_with.length(), replace_with);
  cout << "F" << endl;

  return new_path;
}


int rename_dir(string username, string old_dir_name, string new_dir_name, KeyValueClient &client) {
  cout << "OLD DIR NAME: " << old_dir_name << endl;
  cout << "NEW DIR NAME: " << new_dir_name << endl;

  Document outDoc;
  outDoc.SetObject();
  Document::AllocatorType& allocator = outDoc.GetAllocator();
  auto files_and_dirs_strings_pair = get_all_files_json(username, old_dir_name, allocator, client);
  string all_files_string = files_and_dirs_strings_pair.first;
  string all_dirs_string = files_and_dirs_strings_pair.second;
  auto all_files_vec = separate_string(all_files_string, ";");
  auto all_dirs_vec = separate_string(all_dirs_string, ";");


  auto old_dir_split = separate_string(old_dir_name, "/");
  old_dir_split[old_dir_split.size() - 1] = new_dir_name;
  string new_dir_path = "";
  for (unsigned long i = 0; i < old_dir_split.size(); i++) {
    new_dir_path.append("/");
    new_dir_path.append(old_dir_split[i]);
  }


  vector<pair<string, string>> files_to_write;
  for (unsigned long i = 0; i < all_files_vec.size(); i++) {
    string new_file_path = replace_in_path(all_files_vec[i], old_dir_name, new_dir_path);
    string file_data = get_file_data(username, all_files_vec[i], client);
    pair<string, string> new_pair(new_file_path, file_data);
    files_to_write.push_back(new_pair);
  }

  vector<string> dirs_to_write;
  cout << 592 << endl;
  for (int i = 0; i < all_dirs_vec.size(); i++) {
    cout << 594 << endl;
    cout << all_dirs_vec[i] << endl;
    cout << old_dir_name<< endl;
    cout << new_dir_path << endl;

    string x = replace_in_path(all_dirs_vec[i], old_dir_name, new_dir_path);
    dirs_to_write.push_back(x);
  }


  delete_dir(username, old_dir_name, client);


  for (unsigned long i = 0; i < dirs_to_write.size(); i++) {
    string dir_to_build(dirs_to_write[i]);
    dir_to_build.append("/x");
    build_out_directory_path(username, dir_to_build, client);
  }


  for (unsigned long i = 0; i < files_to_write.size(); i++) {
    write_file(username, files_to_write[i].first, files_to_write[i].second, client);
  }



  return 0;
}









//
