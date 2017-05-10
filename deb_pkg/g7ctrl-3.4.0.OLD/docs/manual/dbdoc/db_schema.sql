CREATE TABLE tbl_track (
  'fld_key' INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, 
  'fld_timestamp' INTEGER NOT NULL, 
  'fld_deviceid' INTEGER NOT NULL, 
  'fld_datetime' INTEGER NOT NULL, 
  'fld_lat' TEXT NOT NULL, 
  'fld_lon' TEXT NOT NULL, 
  'fld_approxaddr' TEXT NOT NULL
  'fld_speed' INTEGER NOT NULL, 
  'fld_heading' INTEGER, NOT NULL,  
  'fld_altitude' INTEGER NOT NULL, 
  'fld_satellite' INTEGER NOT NULL, 
  'fld_event' INTEGER NOT NULL, 
  'fld_voltage' TEXT NOT NULL, 
  'fld_detachstat' INTEGER NOT NULL);

CREATE TABLE tbl_info (
  'fld_key' INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, 
  'fld_created' TEXT NOT NULL, 
  'fld_dbversion' INTEGER NOT NULL);

CREATE TABLE tbl_device_nick ( 
  'fld_nick' TEXT NOT NULL,
  'fld_devid' INTEGER NOT NULL,
  'fld_imei' INTEGER PRIMARY KEY NOT NULL,
  'fld_sim' TEXT NOT NULL,
  'fld_phone' TEXT NOT NULL,
  'fld_fwver' TEXT NOT NULL,
  'fld_regdate' TEXT NOT NULL,
  'fld_upddate' TEXT NOT NULL);