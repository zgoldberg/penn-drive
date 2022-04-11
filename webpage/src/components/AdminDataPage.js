import $ from 'jquery';
import {useState, useEffect} from 'react';
import { Link, Routes, Route, Outlet } from 'react-router-dom';
import '../Data.css';
import NavBar from './NavBar';

function AdminDataPage() {
    const [pageIndex, setPageIndex] = useState(0);
    const [numCols, setNumCols] = useState(0);
    const [nextPageExists, setNextPageExists] = useState(false);
    const [rowData, setRowData] = useState([]);
    const [cellDump, setCellDump] = useState("");

    function RowView(props) {
        let numCols = props.numCols;
        let rowObj = props.rowObj;
        return (
            <div className="tab-row">
                <div className="tab-col">
                    {rowObj.user ? rowObj.user : ""}
                </div>
                {Array(numCols).fill(undefined).map(
                    (item, idx) =>
                        (idx < rowObj.cols.length) ?
                            ((!rowObj.user) ?
                                <div className="tab-col">
                                    {rowObj.cols[idx]}
                                </div>
                            :
                                <div className="tab-col">
                                    {/*<Link to="/emails">
                                        {rowObj.cols[idx]}
                                    </Link>*/}
                                    <button
                                         onClick={(e) => getValue(e, {row: rowObj.user, col: rowObj.cols[idx]})}>
                                        {rowObj.cols[idx]}
                                    </button>
                                </div>
                            )
                        :
                        <div className="tab-col"></div>
                )}
            </div>
        );
    }

    useEffect(() => {
        $.ajax({
            url: `http://localhost:8000/rowsData?pageIndex=${pageIndex}`,
            method: 'GET',
            crossDomain: true,
            dataType: 'json',
            success: (res) => {
                setRowData(res.data);

                let max_cols = 0;
                for (let i = 0; i < res.data.length; i++) {
                    max_cols = Math.max(max_cols, res.data[i].cols.length);
                }
                setNumCols(max_cols);
                setNextPageExists(res.next_page);
            }
        });
    }, [pageIndex]);

    function getValue(e, item) {
        let rowName = item.row;
        let colName = item.col;
        $.ajax({
            url: `http://localhost:8000/cellData?rowName=${rowName}&colName=${colName}`,
            method: 'GET',
            crossDomain: true,
            dataType: 'json',
            success: (res) => {
                setCellDump(res.data);
            }
        });
    }

    function incrementPage() {
        setPageIndex(pageIndex + 1);
    }

    function decrementPage() {
        setPageIndex(Math.max(0, pageIndex - 1));
    }

    return (
        <>
            <NavBar />
            <div className="header">Raw Table Data</div>
            <div>Page #: {pageIndex + 1}</div>
            <div className="button_container">
                {(pageIndex > 0) ?
                    <button className="prev_btn" onClick={() => decrementPage()}>
                        Prev
                    </button>
                :
                    <></>
                }
                {(nextPageExists === "true") ?
                    <button className="next_btn" onClick={() => incrementPage()}>
                        Next
                    </button>
                :
                    <></>
                }
            </div>
            <div className="table">
                <RowView
                    rowObj={{
                        user: "",
                        "cols": Array(numCols).fill(undefined).map((item, idx) => idx + 1)
                    }}
                    numCols={numCols} />
                {rowData.map((rowObj, index) =>
                    // rowObj = { user: "username", cols: [col1, col2] }
                    <RowView rowObj={rowObj} numCols={numCols} />
                )}
            </div>
            <div>{cellDump}</div>
        </>
    );
}

export default AdminDataPage;
