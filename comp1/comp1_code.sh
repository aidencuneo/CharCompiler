' >' < P < P

# Tokenise the user's input
, :
    =c

    # If current char is a space
    $c r1 aaa2 -- !?
        Fs
    ;

    # If current char is not a space
    $c r1 aaa2 -- ?
        $c >
    ;

    ,
;

# Add the final null byte for the last part of the user's input
Fs

# Loop through the tokenised string and search for the word 'string'

0 @c =m
0=i

# While i != m
$i r1 $m -- :
    # Get array item
    $i > Fg

    0 ra # Length
    0 rb # (bool) Break the loop now?

    # Get length of array item while checking if it's equal to 'string'
    < :
        =c

        # Increment length
        Ra 1 ra

        # Now comes the hardcoded equality checks for each letter

        # If length == 1 (1st letter)
        1 r1 Ra -- !?
            $c r1 's' < -- rb
        ;

        # If length == 2 (2nd letter)
        2 r1 Ra -- !?
            $c r1 't' < -- rb
        ;

        # If length == 3 (3rd letter)
        3 r1 Ra -- !?
            $c r1 'r' < -- rb
        ;

        # If length == 4 (4th letter)
        4 r1 Ra -- !?
            $c r1 'i' < -- rb
        ;

        # If length == 5 (5th letter)
        5 r1 Ra -- !?
            $c r1 'n' < -- rb
        ;

        # If length == 6 (6th letter)
        6 r1 Ra -- !?
            $c r1 'g' < -- rb
        ;

        # Make sure length hasn't reached 7 yet (7 is the length of 'string')
        07 r1 Ra -- ?
            # Make sure the loop isn't meant to break yet
            Rb !?
                <
            ;
        ;
    ;

    0 re # (bool) Is this array item equal to 'string'?

    # If length == 6 and !break, then this array item is 'string'
    6 r1 Ra -- !?
        Rb ! re
    ;

    Re ?
        0 > 'gnirts dias uoY' < : P < ;
        0aP
    ;

    # Replace the null byte
    >

    $i 1 =i

    # Condition
    $i r1 $m --
;

# (Assuming it's a string) print it out
# < : P < ; aP
