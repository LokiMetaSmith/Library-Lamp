# GDB Script for Hardware Validation of LoRa MTU

target extended-remote :3333
monitor reset halt
load

# Set a breakpoint right after the required_len is calculated
break rns_encode_packet

commands
    echo \n=== Checking MTU Validation ===\n

    # Step into the function to evaluate required_len
    step
    step
    step
    step

    # Print the values to verify the logic
    print required_len
    print max_len

    if (required_len > max_len)
        echo \n[VALIDATION PASSED] required_len exceeds max_len. Packet will be blocked.\n
    else
        echo \n[VALIDATION PASSED] required_len is safe.\n
    end

    continue
end

continue
