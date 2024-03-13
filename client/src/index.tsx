/* @refresh reload */
import { render } from 'solid-js/web';

import './index.css';
import App from './App';
import { glMatrix } from 'gl-matrix';
import { Route, Router } from '@solidjs/router';
import { Component } from 'solid-js';
import WithParticipantId from './WithParticipantId';
import NasaTLXQuestionnaire from './NasaTLXQuestionnaire';
import Condition from './Condition';
import FinalQuestionnaire from './FinalQuestionnaire';
import Home from './Home';

glMatrix.setMatrixArrayType(Array);

const root = document.getElementById('root');

if (import.meta.env.DEV && !(root instanceof HTMLElement)) {
  throw new Error(
    'Root element not found. Did you forget to add it to your index.html? Or maybe the id attribute got misspelled?',
  );
}

render(
    () => 
        <WithParticipantId>
        {
            participantId =>
                <Router
                    root={({ children }) => <App participantId={participantId}>{children}</App>}
                    base="/interactive-3d-streaming"
                >
                    <Route path="/" component={() => <Home participantId={participantId} />} />
                    <Route
                        path="/cond1-trial"
                        component={() => <Condition participantId={participantId} condition={1} trial />}
                    />
                    <Route
                        path="/cond1-run"
                        component={() => <Condition participantId={participantId} condition={1} />}
                    />
                    <Route
                        path="/cond1-questionnaire"
                        component={
                            () => <NasaTLXQuestionnaire
                                participantId={participantId}
                                id="/cond1-questionnaire"
                                action="/cond2-trial"
                            />
                        }
                    />

                    <Route
                        path="/cond2-trial"
                        component={() => <Condition participantId={participantId} condition={2} trial />}
                    />
                    <Route
                        path="/cond2-run"
                        component={() => <Condition participantId={participantId} condition={2} />}
                    />
                    <Route
                        path="/cond2-questionnaire"
                        component={
                            () => <NasaTLXQuestionnaire
                                participantId={participantId}
                                id="/cond2-questionnaire"
                                action="/cond3-trial"
                            />
                        }
                    />

                    <Route
                        path="/cond3-trial"
                        component={() => <Condition participantId={participantId} condition={3} trial />}
                    />
                    <Route
                        path="/cond3-run"
                        component={() => <Condition participantId={participantId} condition={3} />}
                    />
                    <Route
                        path="/cond3-questionnaire"
                        component={
                            () => <NasaTLXQuestionnaire
                                participantId={participantId}
                                id="/cond3-questionnaire"
                                action="/final"
                            />
                        }
                    />

                    <Route
                        path="/final"
                        component={() => <FinalQuestionnaire participantId={participantId} />}
                    />

                </Router>
        }
        </WithParticipantId>,
    root!
);
