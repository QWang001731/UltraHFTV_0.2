echo "adding static arp for port 1..."
sudo ip neigh replace 10.10.10.2 lladdr 34:b3:54:0f:69:b0 nud permanent dev enp7s0f0

echo "Bind interfaceg enp7s0f1 to dpdk using "
bash ./bind_enp7s0f1_to_dpdk.sh

gnome-terminal -- bash -c '
    cd ~/ultraHFT
    sudo taskset -c 6 ./dpdk_market_data_handler
    exec bash
'
echo "start dpdk parser on a new terminal"

#tid=$(pgrep -o dpdk_market_dat)

while ! pgrep -x 'dpdk_market_dat' > /dev/null; do
 	sleep 0.1
done

pid=$(pgrep -o -x 'dpdk_market_dat')

echo "dpdk parser running with tid  $pid"

echo "waiting for dpdk telemetry thread to start "

VAR=10
while [ $VAR -gt 0 ];do
    echo "wait for "$VAR" second"
    sleep 1
    ((VAR--))
done

while read -r TID NAME;do
    case "$NAME" in
        dpdk_market_dat)
         echo "Pinning parser TID $TID to cpu6"
        sudo taskset -pc 6 "$TID"
        ;;
        *)
        echo "Moving $NAME TID $TID to cpu 0-5"
        sudo taskset -pc 0-5 "$TID"
	    ;;
esac

done < <(ps -T -p "$pid" -o tid=,comm=)

echo "starting replayer..."
gnome-terminal -- bash -c '
    cd ~/ultraHFT
    ./itch_replay_sender 01302019.NASDAQ_ITCH50 10.10.10.2 5000 10.10.10.1
    exec bash
'

if false;then
echo "starting perf..."
gnome-terminal -- bash -c '
    cd ~/ultraHFT
    echo "Profiling TID: $1"
    sudo perf stat -t $1 \
    -e context-switches,cpu-migrations,cycles,instructions \
    -- sleep 30
    exec bash
' _ "$pid"
fi
exit 0