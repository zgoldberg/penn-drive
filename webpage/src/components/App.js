import $ from 'jquery';
import {useState, useEffect} from 'react';
import '../App.css';
import Login from './Login';
import { Link, useNavigate } from 'react-router-dom';

function App() {
  const nav = useNavigate();
  const [loading, setLoading] = useState(true);

  useEffect(() => {
      $.ajax({
        // url: `http://${ip_address}:8000`,
        url: `http://localhost:8000`,
        dataType: "json",
        method: 'GET',
        crossDomain: true,
        xhrFields: {
            withCredentials: true,
        },
        success: (res) => {
            console.log(res);
            setLoading(false);
            if (res.auth == 'true') {
                nav('menu');
            }
        },
        error : (err) => {
            console.log('fail');
        },
      });
  }, []);

  return (
    <div>
        {loading ? <></> : <Login />}
    </div>
  );
}

export default App;
