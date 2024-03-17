import { Component } from "solid-js";
import { render } from "solid-js/web";
import LineSettings from "./settings";

const root = document.getElementById("root");

const App : Component = () =>
{



    return (
        <div>
            <h1>asdasasddasd</h1>
            <LineSettings>

            </LineSettings>
        </div>
    );
};

render(() => App({}), root!);