import React from 'react';
import { Routes, Route } from 'react-router-dom';

import App from './components/App';
import EmailsPage from './components/EmailsPage';
import EmailView from './components/EmailView';
import UploadFile from './components/UploadFile';
import ViewFiles from './components/ViewFiles';
import Register from './components/Register';
import AdminPage from './components/AdminPage';
import AdminDataPage from './components/AdminDataPage';
import Menu from './components/Menu';
import Account from './components/Account';
import ChangePassword from './components/ChangePassword';

export default (
<Routes>
  <Route exact path="/" element={<App/>} />
  <Route path="/emails" element={<EmailsPage sent_mode={false}/>}>
    <Route path=":hash_link" element={<EmailView />} />
  </Route>
  <Route path="/sent_emails" element={<EmailsPage sent_mode={true}/>}>
    <Route path=":hash_link" element={<EmailView />} />
  </Route>
  <Route path="/upload_file" element={<UploadFile/>} />
  <Route path="/files" element={<ViewFiles/>} />
  <Route path="/new_account" element={<Register />} />
  <Route path="/admin" element={<AdminPage />} />
  <Route path="/admin_data" element={<AdminDataPage />}>
    <Route path=":row/:col" element={<EmailView />} />
  </Route>
  <Route path="/menu" element={<Menu />} />
  <Route path="/account" element={<Account />} />
  <Route path="/change_password" element={<ChangePassword />} />
</Routes>
);
