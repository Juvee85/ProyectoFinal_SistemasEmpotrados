Create database Terrario;
use Terrario;

CREATE TABLE lecturas (
  id INT AUTO_INCREMENT PRIMARY KEY,
  fecha DATETIME,
  sensor VARCHAR(20),
  minima FLOAT,
  maxima FLOAT,
  actual FLOAT
);