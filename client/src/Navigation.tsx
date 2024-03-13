import { createSignal, type Component, Show, createEffect, createMemo } from 'solid-js';
import { Button, ListGroup } from 'solid-bootstrap';
import './Navigation.css';
import { SERVER_URL } from './App';
import { Location, useLocation, useNavigate, useParams } from '@solidjs/router';
import { useLogout } from './WithParticipantId';

interface Progress {
    "cond1-trial": boolean;
    "cond1-run": boolean;
    "cond1-questionnaire": boolean;
    "cond2-trial": boolean;
    "cond2-run": boolean;
    "cond2-questionnaire": boolean;
    "cond3-trial": boolean;
    "cond3-run": boolean;
    "cond3-questionnaire": boolean;
    "final": boolean;
}

const ORDER = [
    "cond1-trial",
    "cond1-run",
    "cond1-questionnaire",
    "cond2-trial",
    "cond2-run",
    "cond2-questionnaire",
    "cond3-trial",
    "cond3-run",
    "cond3-questionnaire",
    "final",
];

const NavigationItem: Component<{
    name: string,
    path: string,
    progress: () => Progress | undefined,
    location: Location,
}> = ({ path, progress, location, name }) => {
    const navigate = useNavigate();

    const disabled = createMemo(() => {
        const index = ORDER.indexOf(path.substring(1));
        if (index === 0) {
            return false;
        } else {
            return !progress()?.[ORDER[index - 1]];
        }
    });

    return (
        <ListGroup.Item
            active={location.pathname.endsWith(path)}
            action
            onClick={() => navigate(path)}
            variant={
                progress()?.[path.substring(1)] ? "success" : undefined
            }
            disabled={disabled()}
        >
            {name}
        </ListGroup.Item>
    );
}

const Navigation: Component<{ participantId: number}> = ({ participantId }) => {
    const location = useLocation();
    const [progress, setProgress] = createSignal<Progress>();
    const logout = useLogout();
    const navigate = useNavigate();

    createEffect(() => {
        // The following statement seems useless but is important to let the effect
        // run whenever the page changes.
        location.pathname;
        fetch(`${SERVER_URL}/progress?participantId=${participantId}`)
            .then(async response => {
                const json = await response.json();
                setProgress(json as Progress);
            })
            .catch(console.error);
    });

    // const finalAvailable = createMemo(() =>
    //     progress()?.['cond1-trial'] &&
    //     progress()?.['cond1-run'] &&
    //     progress()?.['cond1-questionnaire'] &&
    //     progress()?.['cond2-trial'] &&
    //     progress()?.['cond2-run'] &&
    //     progress()?.['cond2-questionnaire'] &&
    //     progress()?.['cond3-trial'] &&
    //     progress()?.['cond3-run'] &&
    //     progress()?.['cond3-questionnaire']
    // );
    // createEffect(() => console.log(finalAvailable()));

    return (
        <div style={{ "text-align": "center"}}>
            <div style={{ display: "flex", "flex-direction": "column", margin: "0.5em" }}>
                <h6>Participant { participantId }</h6>
                <Button
                    size="sm"
                    variant="outline-primary"
                    onClick={() => {
                        if (logout) {
                            logout();
                            navigate('/');
                        }
                    }}
                >
                    End Run
                </Button>
            </div>
            <ListGroup class="list-group-root">
                <ListGroup.Item>Condition 1</ListGroup.Item>
                <ListGroup>
                    <NavigationItem name="Trial" path="/cond1-trial" progress={progress} location={location} />
                    <NavigationItem name="Run" path="/cond1-run" progress={progress} location={location} />
                    <NavigationItem name="Questionnaire" path="/cond1-questionnaire" progress={progress} location={location} />
                </ListGroup>

                <ListGroup.Item>Condition 2</ListGroup.Item>
                <ListGroup>
                    <NavigationItem name="Trial" path="/cond2-trial" progress={progress} location={location} />
                    <NavigationItem name="Run" path="/cond2-run" progress={progress} location={location} />
                    <NavigationItem name="Questionnaire" path="/cond2-questionnaire" progress={progress} location={location} />
                </ListGroup>

                <ListGroup.Item>Condition 3</ListGroup.Item>
                <ListGroup>
                    <NavigationItem name="Trial" path="/cond3-trial" progress={progress} location={location} />
                    <NavigationItem name="Run" path="/cond3-run" progress={progress} location={location} />
                    <NavigationItem name="Questionnaire" path="/cond3-questionnaire" progress={progress} location={location} />
                </ListGroup>


                <NavigationItem
                    name="Final"
                    path="/final"
                    progress={progress}
                    location={location}
                />
            </ListGroup>
        </div>
    );
};

export default Navigation;
