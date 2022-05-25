/*
Para la reconstruccion usas la velocidad del pixel, multiplicas por el delta time, y tenes una estimacion 
de donde deberia haber terminado ese pixel, entonces escribis ahi el pixel que tenes ahora (podes usar alpha blending con add, para promediar, dividis entre alpha)
Esto te va a dejar algunos pixels sin completar, ahi pones una red que invente que pixels pueden corresponder

Podria agregar ademas una pasada final de "correccion", por el tema de cosas como iluminacion y eso que cambian tambien
*/