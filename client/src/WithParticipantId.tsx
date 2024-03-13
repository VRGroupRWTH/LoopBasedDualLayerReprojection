import { createSignal, Show, FlowComponent, JSXElement, createEffect, onCleanup, createContext, useContext } from 'solid-js';
import './Navigation.css';
import ParticipantIdForm from './ParticipantIdForm';

const valueToParticipantId = (value: string | null) => {
    if (value) {
        return parseInt(value);
    } else {
        return undefined;
    }
};

interface Participant {
    id: number;
    logout: () => void;
}
const ParticipantContext = createContext<Participant>();
const SESSION_STORAGE_KEY = "participantId";

const useLogout = () => {
    const participant = useContext(ParticipantContext);
    return participant?.logout;
}

const WithParticipantId: FlowComponent<{}, (participantId: number) => JSXElement> = ({ children }) => {
    const [participantId, setParticipantId] = createSignal(valueToParticipantId(sessionStorage.getItem(SESSION_STORAGE_KEY)));

    const logout = () => {
        setParticipantId();
        sessionStorage.removeItem(SESSION_STORAGE_KEY);
    };

    return (
        <Show
            when={participantId()}
            fallback={
                <ParticipantIdForm
                    setParticipantId={participantId => {
                        setParticipantId(participantId);
                        sessionStorage.setItem("participantId", participantId.toString());
                    }}
                />
            }
        >
            {
                participantId => 
                    <ParticipantContext.Provider
                        value={{
                            id: participantId(),
                            logout,
                        }}
                    >
                        { children(participantId()) }
                    </ParticipantContext.Provider>
            }
        </Show>
    );
};

export default WithParticipantId;
export { useLogout };
