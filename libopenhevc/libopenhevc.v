LIBOPENHEVC_MAJOR {
    global:
        oh_*;
    local:
        *;
};
#TODO: the av* global symbol will be to remove from lib since we don't want to call ffmpeg functions from this lib it might create conflicts when used with external ffmpeg
