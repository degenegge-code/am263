"# am263" 

zatim funguje:
- Chceme aby H-můstek použil jednu epWM jednu a oba výstupy A+B, kdy použijeme symetrickou PWM (čítáme nahoru a dolů) a puls na výstupu A realizujeme pomocí události ZERO (AQ nastavíme na přepni do 1 na výstupu A) a události COMP A (shoda komparačního registru A), kdy AQ nastavíme na přepni do log0 na výstupu A. Puls pro výstup B (sepnutí druhé větve, myšleno křížen přes H-můstek sepni dva prvky) realizujeme pomocí události „start čítání dolů“, kdy AQ provede přepni na 1 na výstupu B a událost COMP B provede přepnutí do 0 na výstupu B. Frekvenci chceme pro tento školní případ 50kHz. Šířku pulsu chceme stejnou pro výstup A i B a to cca 25% délky periody. Pulsy pro usměrňovač pak, provedeme pomocí další epWM jednotky, kterou budeme synchronizovat s první ePWM jednotkou, ale s posunem cca 5% periody. Způsob generování je shodný s pulsy pro HBR. Šířku pulsu chceme tak aby puls končil před koncem pulsu pro HBR o cca 5%. Chceme to vidět na osciloskopu.
- Připojíme převodník na eCAP jednotku procesoru. Chceme realizovat nastavení eCAP jednotky tak, bude kontinuálně „sama“ měřit frekvenci a CPU, pokud bude chtít, tak se zcela asynchronně podívá kamsi do registru a přečte si aktuální frekvenci. Cílem je nulové využití CPU pro měření(žádné INT rutiny). (čtu si vlastní pwmku, jako že to je čidlo)
- Eqep pro zjišťování r\chlosti u irc čidla -hotovo


TOTO NELZE:
-  Ověření funkce COMP C a COMP D. Nový CPU má dva komparační registry navíc. Jako test bych navrhoval jen předělat předchozí případ, kdy místo A a B použijeme C a D.

TODO:
- Ověřit čtení čidla polohy, které hodnotu předává asynchronní nebo synchronní komunikací. Tj. buď použít UART pro asynchronní komunikaci. Chceme nulovou spotřebu CPU. Tj. nastavit UART, aby příchozí hodnoty ukládal přes sebe na jedno místo, které budeme číst.  Pokud je komunikace synchronní, tak použít SPI jednotku a nebo PRU-ICSSG (pokud ta sběrnice bude nějaká divná).
- u epwemek ověřit ty int nums atd

wtfs:
[33]ModuleNotFoundError: No module named 'cryptography'
[34]gmake[3]: *** [makefile_ccs_bootimage_gen:97: all] Error 1
[35]gmake[2]: [makefile:169: post-build] Error 2 (ignored)


